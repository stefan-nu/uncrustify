#!/usr/bin/env python
#
# Scans the .test files on the command line and parses each, running
# the tests listed.  Results are printed out.
#
# This could all be done with bash, but I wanted to use python. =)
# Anyway, this was all done while waiting in the Denver airport.
# * @author  Guy Maurel since version 0.62 for uncrustify4Qt
# *          October 2015
#

import argparse
import sys
import os
import time
import timeit
import filecmp
import thread
import threading
from sys import stdout
from threading import Thread, Lock, current_thread
from mutex import mutex
from xmllib import starttagend


# OK, so I just had way too much fun with the colors..

starttime = timeit.default_timer()

# windows doesn't support ansi sequences (unless using ConEmu and enabled)
disablecolors = os.name == "nt" and os.environ.get('CONEMUANSI', '') != 'ON'

if disablecolors:
    NORMAL      = ""
    BOLD        = ""
    UNDERSCORE  = ""
    REVERSE     = ""
else:
    NORMAL      = "\033[0m"
    BOLD        = "\033[1m"
    UNDERSCORE  = "\033[1m"
    REVERSE     = "\033[7m"

FG_BLACK    = "\033[30m"
FG_RED      = "\033[31m"
FG_GREEN    = "\033[32m"
FG_YELLOW   = "\033[33m"
FG_BLUE     = "\033[34m"
FG_MAGNETA  = "\033[35m"
FG_CYAN     = "\033[36m"
FG_WHITE    = "\033[37m"

FGB_BLACK   = "\033[90m"
FGB_RED     = "\033[91m"
FGB_GREEN   = "\033[92m"
FGB_YELLOW  = "\033[93m"
FGB_BLUE    = "\033[94m"
FGB_MAGNETA = "\033[95m"
FGB_CYAN    = "\033[96m"
FGB_WHITE   = "\033[97m"


BG_BLACK    = "\033[40m"
BG_RED      = "\033[41m"
BG_GREEN    = "\033[42m"
BG_YELLOW   = "\033[43m"
BG_BLUE     = "\033[44m"
BG_MAGNETA  = "\033[45m"
BG_CYAN     = "\033[46m"
BG_WHITE    = "\033[47m"

BGB_BLACK   = "\033[100m"
BGB_RED     = "\033[101m"
BGB_GREEN   = "\033[102m"
BGB_YELLOW  = "\033[103m"
BGB_BLUE    = "\033[104m"
BGB_MAGNETA = "\033[105m"
BGB_CYAN    = "\033[106m"
BGB_WHITE   = "\033[107m"

# after all that, I chose c

if disablecolors:
    FAIL_COLOR     = ""
    PASS_COLOR     = ""
    MISMATCH_COLOR = ""
    UNSTABLE_COLOR = ""
    SKIP_COLOR     = ""
else:
    FAIL_COLOR     = UNDERSCORE
    PASS_COLOR     = FG_GREEN
    MISMATCH_COLOR = FG_RED #REVERSE
    UNSTABLE_COLOR = FGB_CYAN
    SKIP_COLOR     = FGB_YELLOW

# global variables that are used to communicate between threads
# every thread may read and updated those share variables
# To prevent corruption writing is only allowed when holding the mutex
pass_count   = 0
fail_count   = 0
unst_count   = 0
test_count   = 0
thread_count = 0
mutex = Lock()

def printf(format, *args):
    sys.stdout.write(format % args)

def run_tests(args, test_name, config_name, input_name, lang):
    global unst_count
    global pass_count
    global thread_count
    
    # mutex.acquire()
    # print("Test:  ", test_name)
    # print("Config:", config_name)
    # print("Input: ", input_name)
    # print('Output:', expected_name)
    # mutex.release()

    if not os.path.isabs(config_name):
        config_name = os.path.join('config', config_name)

    if test_name[-1] == '!':
        test_name = test_name[:-1]
        rerun_config = "%s.rerun%s" % os.path.splitext(config_name)
    else:
        rerun_config = config_name

    expected_name = os.path.join(os.path.dirname(input_name), test_name + '-' + os.path.basename(input_name))
    resultname = os.path.join(args.results, expected_name)
    outputname = os.path.join('output', expected_name)
    try:
        os.makedirs(os.path.dirname(resultname))
    except:
        pass

    cmd = '"%s" -q -c %s -f input/%s %s -o %s %s' % (args.exe, config_name, input_name, lang, resultname, "-LA 2>" + resultname + ".log -p " + resultname + ".unc" if args.g else "-L1,2")
    if args.c:
        print("RUN: " + cmd)

    a = os.system(cmd)
    if a != 0:
        mutex.acquire()
        print(FAIL_COLOR + "FAILED:   " + NORMAL + test_name + " " + input_name)
        global fail_count
        fail_count   += 1
        thread_count -= 1
        mutex.release()
        return -1

    # evaluate if the test output is as expected
    try:
        if not filecmp.cmp(resultname, outputname):
            if args.d:
                cmd = "git diff --no-index %s %s" % (outputname, resultname)
                sys.stdout.flush()
                os.system(cmd)
                
            mutex.acquire()   
            print(UNSTABLE_COLOR + "UNSTABLE: " + NORMAL + test_name + " " + input_name) 
            unst_count   += 1   
            thread_count -= 1
            mutex.release()  
            return -2
    except:
        # impossible
        mutex.acquire()
        print(UNSTABLE_COLOR + "MISSING: " + NORMAL + test_name + " " + input_name)  
        fail_count   += 1   
        thread_count -= 1
        mutex.release() 
        return -1

    if args.p:
        print(PASS_COLOR + "PASSED: " + NORMAL + test_name)
        
    mutex.acquire()
    pass_count += 1
    thread_count -= 1
    mutex.release()    
    return 0

def process_test_file(args, filename):
    global thread_count
    global test_count
    
    # \todo add a command line option verbose to print progress messages
    # \todo add a command line option to set the maximal number of worker threads
    
    # usually a good choice for the number of parallel threads is twice the 
    # number of available CPU cores plus a few extra threads. This leads
    # to fast overall test speed but little thread blocking
    max_threads = 32
    
    fd = open(filename, "r")
    if fd == None:
        print("Unable to open " + filename)
        return None
    print("Processing " + filename)
    for line in fd:
        line = line.strip()
        parts = line.split()
        if (len(parts) < 3) or (parts[0][0] == '#'):
            continue
        if args.r != None:
            test_name = parts[0]
            # remove special suffixes (eg. '!')
            if not test_name[-1].isdigit():
                test_name = test_name[:-1]
            test_nb = int(test_name)
            # parse range list (eg. 10001-10010,10030-10050,10063)
            range_filter = args.r
            filtered = True
            for value in range_filter.split(","):
                t = value.split("-")
                a = b = int(t[0])
                if len(t) > 1:
                    b = int(t[1])
                if test_nb >= a and test_nb <= b:
                    filtered = False
                    break
            if filtered:
                if args.p:
                    print(SKIP_COLOR + "SKIPPED: " + NORMAL + parts[0])
                continue
        lang = ""
        if len(parts) > 3:
            lang = "-l " + parts[3]

        test_count   += 1
        thread_count += 1
        
        # start a worker thread that performs the test
        test = threading.Thread(target=run_tests, args=(args, parts[0], parts[1], parts[2], lang))
        test.start()
        
        # if the maximal allowed number of worker threads got reached
        # wait until at least one worker thread has terminated
        while True:
            if thread_count < max_threads:
                break
            else:
                print("\r(%d / %d) tests finished" % (pass_count, test_count)),
                time.sleep(1)
                
    print("\n")                
    return               

#
# entry point
#
def main(argv):
    global pass_count
    global fail_count
    global unst_count
    global test_count
    global thread_count
    global starttime
    
    all_tests = "c-sharp c cpp d java pawn objective-c vala ecma".split()

    parser = argparse.ArgumentParser(description='Run uncrustify tests')
    parser.add_argument('-c', help='show commands', action='store_true')
    parser.add_argument('-d', help='show diff on failure', action='store_true')
    parser.add_argument('-p', help='show passed/skipped tests', action='store_true')
    parser.add_argument('-g', help='generate debug files (.log, .unc)', action='store_true')
    parser.add_argument('-r', help='specify test filter range list', type=str, default=None)
    parser.add_argument('--results', help='specify results folder', type=str, default='results')
    parser.add_argument('--exe', help='uncrustify executable to test',
                        type=str)
    parser.add_argument('tests', metavar='TEST', help='test(s) to run (default all)',
                        type=str, default=all_tests, nargs='*')
    args = parser.parse_args()

    # Save current working directory from which the script is called to be 
    # able to resolve relative --exe paths
    cwd = os.getcwd()
    os.chdir(os.path.dirname(os.path.realpath(__file__)))

    if not args.exe:
        if os.name == "nt":
            bin_path = '../win32/{0}/uncrustify.exe'
            if args.g:
                bin_path = bin_path.format('Debug')
            else:
                bin_path = bin_path.format('Release')
        else:
            bin_path = '../src/uncrustify'
        args.exe = os.path.abspath(bin_path)
    else:
        if not os.path.isabs(args.exe):
            args.exe = os.path.normpath(os.path.join(cwd, args.exe))

    if not os.path.exists(args.exe):
        print(FAIL_COLOR + "FAILED: " + NORMAL + "Cannot find uncrustify executable")
        return -1

    # do a sanity check on the executable
    cmd = '"%s" > %s' % (args.exe, "usage.txt")
    if args.c:
        print("RUN: " + cmd)
    a = os.system(cmd)
    if a != 0:
        print(FAIL_COLOR + "FAILED: " + NORMAL + "Sanity check")
        return -1

    #print args
    print("Tests: " + str(args.tests))

    for item in args.tests:
        if not item.endswith('.test'):
            item += '.test'
        process_test_file(args, item)

    # wait until all started tests have terminated
    while True:
        if thread_count <= 0:
            break
        else:
            print ("\r(%d / %d) tests remaining" % (thread_count, test_count)),
            time.sleep(1)
    
    # all worker threads have stopped by now, 
    # so we dont need to acquire the mutex from now on
    
    stoptime = timeit.default_timer()
    duration = stoptime - starttime
    
    printf(       "\n-------------------------\n") 
    printf(       "Runtime: %d sec, Time per Test: %2.2fsec \n", duration, (float(duration)/test_count) )
    printf(       "Tests:    %3d \n", test_count)
    printf(       "Passed:   %3d (%3.1f%%) \n", pass_count, (float(pass_count)*100)/test_count )
    printf(BOLD + "Failed:   %3d (%3.1f%%) \n", fail_count, (float(fail_count)*100)/test_count)
    printf(BOLD + "Unstable: %3d (%3.1f%%) \n", unst_count, (float(unst_count)*100)/test_count)
    
    if fail_count > 0:
        sys.exit(1)
    else:
        print(BOLD + "All tests passed" + NORMAL)
        sys.exit(0)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
