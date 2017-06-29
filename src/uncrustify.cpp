/**
 * @file uncrustify.cpp
 * This file takes an input C/C++/D/Java file and reformats it.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#define DEFINE_PCF_NAMES
#define DEFINE_CHAR_TABLE

#include "uncrustify_version.h"
#include "uncrustify_types.h"
#include "char_table.h"
#include "chunk_list.h"
#include "align.h"
#include "args.h"
#include "base_types.h"
#include "brace_cleanup.h"
#include "braces.h"
#include "backup.h"
#include "combine.h"
#include "compat.h"
#include "detect.h"
#include "defines.h"
#include "indent.h"
#include "keywords.h"
#include "logger.h"
#include "log_levels.h"
#include "lang_pawn.h"
#include "md5.h"
#include "newlines.h"
#include "output.h"
#include "options.h"
#include "parens.h"
#include "space.h"
#include "semicolons.h"
#include "sorting.h"
#include "tokenize.h"
#include "tokenize_cleanup.h"
#include "token_names.h"
#include "unc_ctype.h"
#include "uncrustify.h"
#include "unicode.h"
#include "universalindentgui.h"
#include "width.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <vector>
#include <deque>

#ifdef HAVE_UNISTD_H
   #include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
   #include <sys/stat.h>
#endif

#ifdef HAVE_STRINGS_H
   #include <strings.h> /* provides strcasecmp() */
#endif

#ifdef HAVE_UTIME_H
   #include <time.h>
#endif


/** type to map a programming language to a typically used filename extension */
struct lang_ext_t
{
   const char* ext;  /**< filename extension typically used for ... */
   const char* name; /**< a programming language */
};


/** struct links a language type with a human readable language name */
struct lang_name_t
{
   const char* name; /**< human readable identification string */
   lang_t      lang; /**< language identifier */
};


static const lang_name_t language_names[] =
{
   { "C",    LANG_C    },
   { "CPP",  LANG_CPP  },
   { "D",    LANG_D    },
   { "CS",   LANG_CS   },
   { "VALA", LANG_VALA },
   { "JAVA", LANG_JAVA },
   { "PAWN", LANG_PAWN },
   { "OC",   LANG_OC   },
   { "OC+",  LANG_CPPO },
   { "ECMA", LANG_ECMA },
};


/** list of known filename extensions linked to the
 *  corresponding programming language */
const struct lang_ext_t language_exts[] =
{
   { ".c",    "C"    }, /* \todo use lang_t instead of strings */
   { ".cpp",  "CPP"  },
   { ".d",    "D"    },
   { ".cs",   "CS"   },
   { ".vala", "VALA" },
   { ".java", "JAVA" },
   { ".pawn", "PAWN" },
   { ".p",    "PAWN" },
   { ".sma",  "PAWN" },
   { ".inl",  "PAWN" },
   { ".h",    "CPP"  },
   { ".cxx",  "CPP"  },
   { ".hpp",  "CPP"  },
   { ".hxx",  "CPP"  },
   { ".cc",   "CPP"  },
   { ".cp",   "CPP"  },
   { ".C",    "CPP"  },
   { ".CPP",  "CPP"  },
   { ".c++",  "CPP"  },
   { ".di",   "D"    },
   { ".m",    "OC"   },
   { ".mm",   "OC+"  },
   { ".sqc",  "C"    }, /**< embedded SQL */
   { ".es",   "ECMA" },
};


/** tbd */
static lang_t language_flags_from_name(
   const char* tag /**< [in]  */
);


/**
 * \brief load a file that provides a header used to be added to output files
 *
 * @retval true  file was loaded successfully
 * @retval false file could not be loaded
 */
static bool load_header_file(
   uo_t       option,   /**< [in]  option that corresponds to file to load */
   file_mem_t &file_mem /**< [out] files descriptions structure to update */
);


/**
 * \brief Find the language for the file extension
 * Default to C
 *
 * @return  LANG_xxx
 */
static lang_t language_flags_from_filename(
   const char* filenme /**< [in] The name of the file */
);


/**
 * \brief Gets the tag text for a language
 *
 * @return  A string
 */
const char* language_name_from_flags(
   uint32_t lang /**< [in] The LANG_xxx enum */
);


/** \brief tbd */
static bool read_stdin(
   file_mem_t& fm /**< [out] file description to update */
);


/**
 * \brief create all folders required for a given path
 * if necessary several subfolders are created until
 * the full path was created on disk
 */
static void make_folders(
   const string& filename /**< [in] full path to create */
);


/** \brief tbd */
static void uncrustify_start(
   const deque<uint32_t>& data /**< [in]  */
);


/** \brief tbd */
void uncrustify_end(
   void /**< [in]  */
);


/** \brief tbd */
static bool ends_with(
   const char* filename, /**< [in]  */
   const char* tag,      /**< [in]  */
   bool case_sensitive   /**< [in]  */
);


/** \brief tbd */
void uncrustify_file(
   const file_mem_t& fm,             /**< [out] file description to update */
   FILE*             pfout,          /**< [in]  */
   const char*       parsed_file,    /**< [in]  */
   bool defer_uncrustify_end = false /**< [in]  */
);


/**
 * \brief Does a source file.
 */
static void do_source_file(
   const char* filename_in,  /**< [in] the file to read */
   const char* filename_out, /**< [in] nullptr (stdout) or the file to write */
   const char* parsed_file,  /**< [in] nullptr or the filename for the parsed debug info */
   bool        no_backup,    /**< [in] don't create a backup, if filename_out == filename_in */
   bool        keep_mtime    /**< [in] don't change the mtime (dangerous) */
);


/** \brief tbd */
static void add_file_header(
   void /**< [in]  */
);


/** \brief tbd */
static void add_file_footer(
   void /**< [in]  */
);


/** \brief tbd */
static void add_func_header(
   c_token_t         type, /**< [in]  */
   const file_mem_t& fm    /**< [out] file description to update */
);


/** \brief tbd */
static void add_msg_header(
   c_token_t         type, /**< [in]  */
   const file_mem_t& fm    /**< [out] file description to update */
);


/** \brief tbd */
static void process_source_list(
   const char* const source_list, /**< [in]  */
   const char*       prefix,      /**< [in]  */
   const char*       suffix,      /**< [in]  */
   bool              no_backup,   /**< [in]  */
   bool              keep_mtime   /**< [in]  */
);


/**
 * \brief load all files that provide headers to be added to files
 *
 * @retval true  file was loaded successfully
 * @retval false file could not be loaded
 */
bool load_all_header_files(
   void /**< [in]  */
);


/** \brief tbd */
static const char* make_output_filename(
   char* buf,                  /**< [in]  */
   const uint32_t    buf_size, /**< [in]  */
   const char* const filename, /**< [in]  */
   const char* const prefix,   /**< [in]  */
   const char* const suffix    /**< [in]  */
);


/**
 * \brief compare the content of two files
 */
static bool file_content_matches(
   const string& filename1, /**< [in] file1 to compare */
   const string& filename2  /**< [in] file2 to compare */
);


/**
 * \brief create the output file name by appending a fixed ending to the input file name
 *
 * typically the ending ".uncrustify" is used
 * example: "source.c" -> "source.c.uncrustify"
 */
static string create_out_filename(
   const char* const filename /**< [in]  */
);


/** tbd */
static bool bout_content_matches(
   const file_mem_t& fm,           /**< [out] file description to update */
   const bool        report_status /**< [in]  */
);


/**
 * \brief Loads a file into memory
 *
 * @retval true  file was loaded successfully
 * @retval false file could not be loaded
 */
static bool load_mem_file(
   const char* const filename, /**< [in]  name of file to load */
   file_mem_t&       fm        /**< [out] file description to update */
);


/**
 * \brief Try to load the file from the config folder first and then by name
 *
 * @retval true  file was loaded successfully
 * @retval false file could not be loaded
 */
static bool load_mem_file_config(
   const char* const filename, /**< [in]  name of file to load */
   file_mem_t&       fm        /**< [out] file description to update */
);


/**
 * \brief print uncrustify version number and terminate
 */
static void version_exit(
   void /**< [in]  */
);


/** \brief tbd */
static void redir_stdout(
   const char* output_file /**< [in]  */
);


/**
 * \brief check if a token reference is valid and holds a valid name
 */
bool is_valid_token_name(
   c_token_t token /**< [in] token to check */
);


/**
 * \brief check if a string pointer is valid and holds a non empty string
 */
bool is_nonempty_string(
  const char* str /**< [in] string to check */
);


/**
 * \brief rename a file
 */
static void rename_file(
   const char* old_name, /**< [in] current file name */
   const char* new_name  /**< [in] changed file name */
);


/** \brief tbd */
static void set_nl_chunk_pos(
   uo_t      check, /**< [in]  */
   uo_t      set,   /**< [in]  */
   c_token_t token  /**< [in]  */
);


cp_data_t cpd;


#ifdef DEFINE_PCF_NAMES
static const char* const pcf_names[] =
{
   "IN_PREPROC",        // 0
   "IN_STRUCT",         // 1
   "IN_ENUM",           // 2
   "IN_FCN_DEF",        // 3
   "IN_FCN_CALL",       // 4
   "IN_SPAREN",         // 5
   "IN_TEMPLATE",       // 6
   "IN_TYPEDEF",        // 7
   "IN_CONST_ARGS",     // 8
   "IN_ARRAY_ASSIGN",   // 9
   "IN_CLASS",          // 10
   "IN_CLASS_BASE",     // 11
   "IN_NAMESPACE",      // 12
   "IN_FOR",            // 13
   "IN_OC_MSG",         // 14
   "#15",               // 15
   "FORCE_SPACE",       // 16
   "STMT_START",        // 17
   "EXPR_START",        // 18
   "DONT_INDENT",       // 19
   "ALIGN_START",       // 20
   "WAS_ALIGNED",       // 21
   "VAR_TYPE",          // 22
   "VAR_DEF",           // 23
   "VAR_1ST",           // 24
   "VAR_INLINE",        // 25
   "RIGHT_COMMENT",     // 26
   "OLD_FCN_PARAMS",    // 27
   "LVALUE",            // 28
   "ONE_LINER",         // 29
   "EMPTY_BODY",        // 30
   "ANCHOR",            // 31
   "PUNCTUATOR",        // 32
   "INSERTED",          // 33
   "LONG_BLOCK",        // 34
   "OC_BOXED",          // 35
   "KEEP_BRACE",        // 36
   "OC_RTYPE",          // 37
   "OC_ATYPE",          // 38
   "WF_ENDIF",          // 39
   "IN_QT_MACRO",       // 40
};
#endif


bool is_lang(lang_t lang)
{
   return((cpd.lang_flags & lang) != 0);
}


bool not_lang(lang_t lang)
{
   return((cpd.lang_flags & lang) == 0);
}


const char* path_basename(const char* path)
{
   retval_if(ptr_is_invalid(path), "");

   const char* last_path = path;
   while (*path != 0) /* check for end of string */ /*lint !e661 */
   {
      /* Check both slash types to support Linux and Windows */
      const char ch = *path;
      path++;
      if ((ch == UNIX_PATH_SEP) || /* \todo better use strcmp */
          (ch == WIN_PATH_SEP ) )
      {
         last_path = path;
      }
   }
   return(last_path);
}


uint32_t path_dirname_len(const char* full_name)
{
   retval_if((full_name == nullptr), 0);

   const char* const file_name = path_basename(full_name);
   /* subtracting addresses like this works only on big endian systems */
   const uint32_t len = (uint32_t)file_name - (uint32_t)full_name;

   return (len);
}


void usage_exit(const char* msg, const char* argv0, int32_t code)
{
   if (ptr_is_valid(msg))
   {
      fprintf(stderr, "%s\n", msg);
      log_flush(true);
   }
   if ((code  != EXIT_SUCCESS) ||
       (ptr_is_invalid(argv0)) )
   {
      fprintf(stderr, "Try running with -h for usage information\n");
      log_flush(true);
      exit(code);
   }
   fprintf(stdout,
           "Usage:\n"
           "%s [options] [files ...]\n"
           "\n"
           "If no input files are specified, the input is read from stdin\n"
           "If reading from stdin, you should specify the language using -l\n"
           "or specify a filename using --assume for automatic language detection.\n"
           "\n"
           "If -F is used or files are specified on the command line,\n"
           "the output filename is 'prefix/filename' + suffix\n"
           "\n"
           "When reading from stdin or doing a single file via the '-f' option,\n"
           "the output is dumped to stdout, unless redirected with -o FILE.\n"
           "\n"
           "Errors are always dumped to stderr\n"
           "\n"
           "The '-f' and '-o' options may not be used with '-F', '--replace' or '--no-backup'.\n"
           "The '--prefix' and '--suffix' options may not be used with '--replace' or '--no-backup'.\n"
           "\n"
           "Basic Options:\n"
           " -c CFG       : Use the config file CFG.\n"
           " -f FILE      : Process the single file FILE (output to stdout, use with -o).\n"
           " -o FILE      : Redirect stdout to FILE.\n"
           " -F FILE      : Read files to process from FILE, one filename per line (- is stdin).\n"
           " --check      : Do not output the new text, instead verify that nothing changes when\n"
           "                the file(s) are processed.\n"
           "                The status of every file is printed to stderr.\n"
           "                The exit code is EXIT_SUCCESS if there were no changes, EXIT_FAILURE otherwise.\n"
           " files        : Files to process (can be combined with -F).\n"
           " --suffix SFX : Append SFX to the output filename. The default is '.uncrustify'\n"
           " --prefix PFX : Prepend PFX to the output filename path.\n"
           " --replace    : Replace source files (creates a backup).\n"
           " --no-backup  : Replace files, no backup. Useful if files are under source control.\n"
           " --if-changed : Write to stdout (or create output FILE) only if a change was detected.\n"
#ifdef HAVE_UTIME_H
           " --mtime      : Preserve mtime on replaced files.\n"
#endif
           " -l           : Language override: C, CPP, D, CS, JAVA, PAWN, OC, OC+, VALA.\n"
           " -t           : Load a file with types (usually not needed).\n"
           " -q           : Quiet mode - no output on stderr (-L will override).\n"
           " --frag       : Code fragment, assume the first line is indented correctly.\n"
           " --assume FN  : Uses the filename FN for automatic language detection if reading\n"
           "                from stdin unless -l is specified.\n"
           "\n"
           "Config/Help Options:\n"
           " -h -? --help --usage     : Print this message and exit.\n"
           " --version                : Print the version and exit.\n"
           " --show-config            : Print out option documentation and exit.\n"
           " --update-config          : Output a new config file. Use with -o FILE.\n"
           " --update-config-with-doc : Output a new config file. Use with -o FILE.\n"
           " --universalindent        : Output a config file for Universal Indent GUI.\n"
           " --detect                 : Detects the config from a source file. Use with '-f FILE'.\n"
           "                            Detection is fairly limited.\n"
           " --set <option>=<value>   : Sets a new value to a config option.\n"
           "\n"
           "Debug Options:\n"
           " -p FILE      : Dump debug info to a file.\n"
           " -L SEV       : Set the log severity (see log_levels.h; note 'A' = 'all')\n"
           " -s           : Show the log severity in the logs.\n"
           " --decode     : Decode remaining args (chunk flags) and exit.\n"
           "\n"
           "Usage Examples\n"
           "cat foo.d | uncrustify -q -c my.cfg -l d\n"
           "uncrustify -c my.cfg -f foo.d\n"
           "uncrustify -c my.cfg -f foo.d -L0-2,20-23,51\n"
           "uncrustify -c my.cfg -f foo.d -o foo.d\n"
           "uncrustify -c my.cfg foo.d\n"
           "uncrustify -c my.cfg --replace foo.d\n"
           "uncrustify -c my.cfg --no-backup foo.d\n"
           "uncrustify -c my.cfg --prefix=out -F files.txt\n"
           "\n"
           "Note: Use comments containing ' *INDENT-OFF*' and ' *INDENT-ON*' to disable\n"
           "      processing of parts of the source file (these can be overridden with \n"
           "      enable_processing_cmt and disable_processing_cmt).\n"
           "\n"
           "There are currently %d options and minimal documentation.\n"
           "Try UniversalIndentGUI and good luck.\n"
           "\n"
           ,
           path_basename(argv0), UO_option_count);
   exit(code);
}


static void version_exit(void)
{
   printf("%s\n", UNCRUSTIFY_VERSION);
   exit(EX_OK);
}


static void redir_stdout(const char* output_file)
{
   const FILE* my_stdout = stdout; /* Reopen stdout */

   if (ptr_is_valid(output_file))
   {
      my_stdout = freopen(output_file, "wb", stdout);
      if (ptr_is_invalid(my_stdout))
      {
         LOG_FMT(LERR, "Unable to open %s for write: %s (%d)\n",
                 output_file, strerror(errno), errno);
         cpd.error_count++;
         usage_exit(nullptr, nullptr, EX_IOERR);
      }
      LOG_FMT(LNOTE, "Redirecting output to %s\n", output_file);
   }
}


int main(int argc, char* argv[])
{
   /* initialize the global data */
   cpd.unc_off_used = false;

   /* check keyword sort */
   assert(keywords_are_sorted());

   /* If ran without options show the usage info and exit */
   if (argc == 1) { usage_exit(nullptr, argv[0], EXIT_SUCCESS); }

   /* make sure we have token_names.h in sync with token_enum.h */
#ifdef DEBUG
   const uint32_t token_name_count = ARRAY_SIZE(token_names);
   const uint32_t ct_token_count   = CT_TOKEN_COUNT_;
   assert(token_name_count == ct_token_count);
#endif

   /* Build options map */
   register_options();

   Args arg_list(argc, argv);
   if (arg_list.Present("--version") || arg_list.Present("-v"))
   {
      version_exit(); /*lint !e522 */
   }
   if (arg_list.Present("--help" ) || arg_list.Present("-h") ||
       arg_list.Present("--usage") || arg_list.Present("-?") )
   {
      usage_exit(nullptr, argv[0], EXIT_SUCCESS);
   }

   if (arg_list.Present("--show-config"))
   {
      print_options(stdout);
      return(EXIT_SUCCESS);
   }

   cpd.do_check   = arg_list.Present("--check"     );
   cpd.if_changed = arg_list.Present("--if-changed");

#ifdef WIN32
   /* tell Windows not to change what I write to stdout */
   UNUSED(_setmode(_fileno(stdout), _O_BINARY));
#endif

   /* Init logging */
   log_init(cpd.do_check ? stdout : stderr);
   log_mask_t mask; /*lint !e1788 */
   if (arg_list.Present("-q"))
   {
      logmask_from_string("", mask);
      log_set_mask(mask);
   }

   const char *p_arg;
   if (((p_arg = arg_list.Param("-L"   )) != nullptr) ||
       ((p_arg = arg_list.Param("--log")) != nullptr) )
   {
      logmask_from_string(p_arg, mask);
      log_set_mask(mask);
   }
   cpd.frag = arg_list.Present("--frag");

   if (arg_list.Present("--decode"))
   {
      uint32_t idx = 1;
      while ((p_arg = arg_list.Unused(idx)) != nullptr)
      {
         log_pcf_flags(LSYS, strtoul(p_arg, nullptr, 16));
      }
      return(EXIT_SUCCESS);
   }

   /* Get the config file name */
   string cfg_file;
   if (((p_arg = arg_list.Param("--config")) != nullptr) ||
       ((p_arg = arg_list.Param("-c"      )) != nullptr) )
   {
      cfg_file = p_arg;
   }
   else if (unc_getenv("UNCRUSTIFY_CONFIG", cfg_file) == false)
   {
      /* Try to find a config file at an alternate location */
      string home;
      if (unc_homedir(home))
      {
         struct stat tmp_stat = {};
         const auto  path0 = home + "/.uncrustify.cfg";
         const auto  path1 = home + "/uncrustify.cfg";
         if      (stat(path0.c_str(), &tmp_stat) == 0) { cfg_file = path0; }
         else if (stat(path1.c_str(), &tmp_stat) == 0) { cfg_file = path1; }
      }
   }

   /* Get the parsed file name */
   const char* parsed_file;
   if (((parsed_file = arg_list.Param("--parsed")) != nullptr) ||
       ((parsed_file = arg_list.Param("-p"      )) != nullptr) )
   {
      LOG_FMT(LNOTE, "Will export parsed data to: %s\n", parsed_file);
   }

   /* Enable log severities */
   if (arg_list.Present("-s"    ) ||
       arg_list.Present("--show") )
   {
      log_show_sev(true);
   }

   /* Load the config file */
   set_option_defaults();

   /* Load type files */
   uint32_t idx = 0;
   while ((p_arg = arg_list.Params("-t", idx)) != nullptr)
   {
      const int32_t return_code = load_keyword_file(p_arg, 256);
      retval_if(return_code != EX_OK, return_code);
   }

   /* add types */
   idx = 0;
   while ((p_arg = arg_list.Params("--type", idx)) != nullptr)
   {
      add_keyword(p_arg, CT_TYPE);
   }

   /* Load define files */
   idx = 0;
   while ((p_arg = arg_list.Params("-d", idx)) != nullptr)
   {
      const int32_t return_code = load_define_file(p_arg, 160);
      retval_if(return_code != EX_OK, return_code);
   }

   /* add defines */
   idx = 0;
   while ((p_arg = arg_list.Params("--define", idx)) != nullptr)
   {
      add_define(p_arg, nullptr);
   }

   /* Check for a language override */
   if ((p_arg = arg_list.Param("-l")) != nullptr)
   {
      cpd.lang_flags = language_flags_from_name(p_arg);
      if (is_lang(LANG_NONE))
      {
         LOG_FMT(LWARN, "Ignoring unknown language: %s\n", p_arg);
      }
      else
      {
         cpd.lang_forced = true;
      }
   }

   /* Get the source file name */
   const char *source_file;
   if (((source_file = arg_list.Param("--file")) == nullptr) &&
       ((source_file = arg_list.Param("-f"    )) == nullptr) )
   {
      // not using a single file, source_file is nullptr
   }

   /* Get a source file list */
   const char *source_list;
   if (((source_list = arg_list.Param("--files")) == nullptr) &&
       ((source_list = arg_list.Param("-F"     )) == nullptr) )
   {
      // not using a file list, source_list is nullptr
   }

   const char* const prefix    = arg_list.Param  ("--prefix");
   const char*       suffix    = arg_list.Param  ("--suffix");
   const char* const assume    = arg_list.Param  ("--assume");
   const bool no_backup        = arg_list.Present("--no-backup");
   const bool replace          = arg_list.Present("--replace");
   const bool keep_mtime       = arg_list.Present("--mtime");
   const bool update_config    = arg_list.Present("--update-config");
   const bool update_config_wd = arg_list.Present("--update-config-with-doc");
   const bool detect           = arg_list.Present("--detect");

   /* Grab the output override */
   const char* output_file = arg_list.Param("-o");

   LOG_FMT(LDATA, "config_file = %s\n", cfg_file.c_str());
   LOG_FMT(LDATA, "output_file = %s\n", (output_file != nullptr) ? output_file : "null");
   LOG_FMT(LDATA, "source_file = %s\n", (source_file != nullptr) ? source_file : "null");
   LOG_FMT(LDATA, "source_list = %s\n", (source_list != nullptr) ? source_list : "null");
   LOG_FMT(LDATA, "prefix      = %s\n", (prefix      != nullptr) ? prefix      : "null");
   LOG_FMT(LDATA, "suffix      = %s\n", (suffix      != nullptr) ? suffix      : "null");
   LOG_FMT(LDATA, "assume      = %s\n", (assume      != nullptr) ? assume      : "null");
   LOG_FMT(LDATA, "replace     = %d\n", replace);
   LOG_FMT(LDATA, "no_backup   = %d\n", no_backup);
   LOG_FMT(LDATA, "detect      = %d\n", detect);
   LOG_FMT(LDATA, "check       = %d\n", cpd.do_check);
   LOG_FMT(LDATA, "if_changed  = %d\n", cpd.if_changed);

   if ((cpd.do_check     == true)   &&
      ( ptr_is_valid(output_file) ||
        ptr_is_valid(prefix)      ||
        ptr_is_valid(suffix)      ||
       (replace          == true) ||
       (no_backup        == true) ||
       (keep_mtime       == true) ||
       (update_config    == true) ||
       (update_config_wd == true) ||
       (detect           == true) ||
        cpd.if_changed            ) )
   {
      usage_exit("Cannot use --check with output options.", argv[0], EX_NOUSER);
   }

   if (!cpd.do_check)
   {
      if ((replace == true) || (no_backup == true))
      {
         if (ptrs_are_valid(prefix, suffix))
         {
            usage_exit("Cannot use --replace with --prefix or --suffix", argv[0], EX_NOINPUT);
         }
         if (ptr_is_valid(source_file) ||
             ptr_is_valid(output_file) )
         {
            usage_exit("Cannot use --replace or --no-backup with -f or -o", argv[0], EX_NOINPUT);
         }
      }
      else
      {
         if (ptrs_are_valid(prefix, suffix))
         {
            suffix = ".uncrustify";
         }
      }
   }

   /* Try to load the config file, if available.
    * It is optional for "--universalindent" and "--detect", but required for
    * everything else. */
   if (!cfg_file.empty())
   {
      cpd.filename = cfg_file.c_str();
      if (load_option_file(cpd.filename) < 0)
      {
         usage_exit("Unable to load the config file", argv[0], EX_IOERR);
      }
      /* test if all options are compatible to each other */
      if (get_uval(UO_nl_max) > 0)
      {
         /* test if one/some option(s) is/are not too big for that */
         if (get_uval(UO_nl_func_var_def_blk) >= get_uval(UO_nl_max))
         {
            fprintf(stderr, "The option 'nl_func_var_def_blk' is too big against the option 'nl_max'\n");
            log_flush(true);
            exit(EX_CONFIG);
         }
      }
   }

#define MAX_ARG_LENGTH 256u
   /* Set config options using command line arguments.*/
   idx = 0;
   while ((p_arg = arg_list.Params("--set", idx)) != nullptr)
   {
      const size_t argLength = strlen(p_arg);
      if (argLength > MAX_ARG_LENGTH)
      {
         fprintf(stderr, "The buffer is to short for the set argument '%s'\n", p_arg);
         log_flush(true);
         exit(EX_SOFTWARE);
      }
      char buffer[MAX_ARG_LENGTH];
      strcpy(buffer, p_arg);

      /* Tokenize and extract key and value */
      const char* token = strtok(buffer, "=");
      const char* const option = token;

      token = strtok(nullptr, "=");
      const char* const value = token;

      if (ptrs_are_valid(option, value) &&
          (strtok(nullptr, "=") == nullptr) ) /* end of argument reached */
      {
         int32_t result = set_option_value(option, value);
         if (result == -1)
         {
            fprintf(stderr, "Unknown option '%s' to override.\n", buffer);
            log_flush(true);
            return(EXIT_FAILURE);
         }
      }
      else
      {
         usage_exit("Error while parsing --set", argv[0], EX_USAGE);
      }
   }

   if (arg_list.Present("--universalindent"))
   {
      FILE* pfile = stdout;

      if (ptr_is_valid(output_file))
      {
         pfile = fopen(output_file, "w");
         if (ptr_is_invalid(pfile))
         {
            fprintf(stderr, "Unable to open %s for write: %s (%d)\n",
                    output_file, strerror(errno), errno);
            log_flush(true);
            return(EXIT_FAILURE);
         }
      }

      print_universal_indent_cfg(pfile);
      fclose(pfile);

      return(EXIT_SUCCESS);
   }

   if (detect)
   {
      file_mem_t fm;

      if (ptrs_are_invalid(source_file, source_list))
      {
         fprintf(stderr, "The --detect option requires a single input file\n");
         log_flush(true);
         return(EXIT_FAILURE);
      }

      /* Do some simple language detection based on the filename extension */
      if (!cpd.lang_forced || is_lang(LANG_NONE))
      {
         cpd.lang_flags = language_flags_from_filename(source_file);
      }

      /* Try to read in the source file */
      if (load_mem_file(source_file, fm) == false)
      {
         LOG_FMT(LERR, "Failed to load (%s)\n", source_file);
         cpd.error_count++;
         return(EXIT_FAILURE);
      }

      uncrustify_start(fm.data);
      detect_options();
      uncrustify_end();

      redir_stdout(output_file);
      save_option_file(stdout, update_config_wd);
      return(EXIT_SUCCESS);
   }

   if (update_config || update_config_wd)
   {
      /* TODO: complain if file-processing related options are present */
      redir_stdout(output_file);
      save_option_file(stdout, update_config_wd);
      return(EXIT_SUCCESS);
   }

   /* Everything beyond this point requires a config file, so complain and
    * bail if we don't have one. */
   if (cfg_file.empty())
   {
      usage_exit("Specify the config file with '-c file' or set UNCRUSTIFY_CONFIG",
                 argv[0], EX_IOERR);
   }

   /* Done parsing args */

   /* Check for unused args (ignore them) */
   idx   = 1;
   p_arg = arg_list.Unused(idx);

   /* Check args - for multi file options */
#if 1 // SN allow debugging with eclipse
   if (ptr_is_valid(source_list) ||
       ptr_is_valid(p_arg      ) )
   {
      if (ptr_is_valid(source_file))
      {
         usage_exit("Cannot specify both the single file option and a multi-file option.",
                    argv[0], EX_NOUSER);
      }

      if (ptr_is_valid(output_file))
      {
         usage_exit("Cannot specify -o with a multi-file option.",
                    argv[0], EX_NOHOST);
      }
   }
#endif

   /* This relies on cpd.filename being the config file name */
   load_all_header_files();

   if (cpd.do_check || cpd.if_changed )
   {
      cpd.bout = new deque<uint8_t>();
   }

   if (ptrs_are_valid(source_file, source_list, p_arg))
   {
      /* no input specified, so use stdin */
      if (is_lang(LANG_NONE))
      {
         cpd.lang_flags = (ptr_is_valid(assume)) ?
           language_flags_from_filename(assume) : (lang_t)LANG_C;
      }

      if (!cpd.do_check) { redir_stdout(output_file); }

      file_mem_t fm;
      if (!read_stdin(fm))
      {
         LOG_FMT(LERR, "Failed to read stdin\n");
         cpd.error_count++;
         return(100);
      }

      cpd.filename = "stdin";

      /* Done reading from stdin */
      LOG_FMT(LSYS, "Parsing: %d bytes (%d chars) from stdin as language %s\n",
              (int32_t)fm.raw.size(), (int32_t)fm.data.size(),
              language_name_from_flags(cpd.lang_flags));

      uncrustify_file(fm, stdout, parsed_file);
   }
   else if (ptr_is_valid(source_file))
   {
      /* Doing a single file */
      do_source_file(source_file, output_file, parsed_file, no_backup, keep_mtime);
   }
   else
   {
      /* Doing multiple files
       * \todo start multiple threads to process several files in parallel */
      if (ptr_is_valid(prefix)) { LOG_FMT(LSYS, "Output prefix: %s/\n", prefix); }
      if (ptr_is_valid(suffix)) { LOG_FMT(LSYS, "Output suffix: %s\n",  suffix); }

      /* Do the files on the command line first */
      idx = 1;
      while ((p_arg = arg_list.Unused(idx)) != nullptr)
      {
         char outbuf[1024];
         do_source_file(p_arg,
               make_output_filename(outbuf, sizeof(outbuf), p_arg, prefix, suffix),
               nullptr, no_backup, keep_mtime);
      }

      if (ptr_is_valid(source_list))
      {
         process_source_list(source_list, prefix, suffix, no_backup, keep_mtime);
      }
   }

   clear_keyword_file();
   clear_defines();

   if ((                           cpd.error_count    != 0 ) ||
       ((cpd.do_check == true) && (cpd.check_fail_cnt != 0)) )
   {
      return(EXIT_FAILURE);
   }

   return(EXIT_SUCCESS);
}


static void process_source_list(const char* const source_list, const char* prefix,
      const char* suffix, const bool no_backup, const bool keep_mtime)
{
   int32_t from_stdin = strcmp(source_list, "-") == 0;
   FILE*   p_file     = from_stdin ? stdin : fopen(source_list, "r");

   if (ptr_is_invalid(p_file))
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, source_list, strerror(errno), errno);
      cpd.error_count++;
      return;
   }

   char    linebuf[256];
   int32_t line = 0;

   while (fgets(linebuf, sizeof(linebuf), p_file) != nullptr)
   {
      line++;
      char*    fname = linebuf;
      uint32_t len   = strlen(fname);
      while ((len > 0) && unc_isspace(*fname))
      {
         fname++;
         len--;
      }
      while ((len > 0) && unc_isspace(fname[len - 1]))
      {
         len--;
      }
      fname[len] = 0;
      while (len-- > 0)
      {
         if (fname[len] == '\\')
         {
            fname[len] = '/';
         }
      }

      LOG_FMT(LFILELIST, "%3d] %s\n", line, fname);

      if (fname[0] != '#')
      {
         char outbuf[1024];
         do_source_file(fname,
               make_output_filename(outbuf, sizeof(outbuf), fname, prefix, suffix),
               nullptr, no_backup, keep_mtime);
      }
   }

   if (!from_stdin) { fclose(p_file); }
}


static bool read_stdin(file_mem_t &fm)
{
   deque<uint8_t> dq;
   uint8_t        buf[4096];

   fm.raw.clear();
   fm.data.clear();
   fm.enc = char_enc_t::ASCII;

   while (!feof(stdin))
   {
      const uint32_t len = fread(buf, 1, sizeof(buf), stdin);
      for (uint32_t idx = 0; idx < len; idx++)
      {
         dq.push_back(buf[idx]);
      }
   }

   fm.raw.clear();
   fm.data.clear();
   fm.enc = char_enc_t::ASCII;

   /* Copy the raw data from the deque to the vector */
   fm.raw.insert(fm.raw.end(), dq.begin(), dq.end());
   return(decode_unicode(fm.raw, fm.data, fm.enc, fm.bom));
}


/**
 * check if a given character is a path separation character
 * of either Windows or Unix type
 */
bool char_is_path_separation(
   const char character   /**< {in] single character to check */
);


bool char_is_path_separation(const char character)
{
   return ((character == UNIX_PATH_SEP) ||
           (character == WIN_PATH_SEP ) );
}


#define FOLDER_RIGHTS 0750 /**< rights used to create a new (sub-)folder */
static void make_folders(const string& filename)
{
   char   outname[4096];
   snprintf(outname, sizeof(outname), "%s", filename.c_str());

   uint32_t start_of_subpath = 0;
   for (uint32_t idx = 0; outname[idx] != 0; idx++)
   {
      /* use the path separation symbol that corresponds to the
       * system uncrustify was build for */
      if(char_is_path_separation(outname[idx]))
      {
         outname[idx] = PATH_SEP;
      }

      if ((idx          > start_of_subpath) && /* search until end of subpath */
          (outname[idx] == PATH_SEP       ) )  /* is found */
      {
         outname[idx] = 0; /* mark the end of the subpath */

         /* create subfolder if it is not the start symbol of a path */
         if ((strcmp(&outname[start_of_subpath], "." ) != 0) &&
             (strcmp(&outname[start_of_subpath], "..") != 0) )
         {
            int32_t status = mkdir(outname, FOLDER_RIGHTS);
            if ((status != 0     ) &&
                (errno  != EEXIST) )
            {
               LOG_FMT(LERR, "%s: Unable to create %s: %s (%d)\n",
                       __func__, outname, strerror(errno), errno);
               cpd.error_count++;
               return;
            }
         }
         outname[idx] = PATH_SEP; /* reconstruct full path to search for next subpath */
      }

      if (outname[idx] == PATH_SEP)
      {
         start_of_subpath = idx + 1;
      }
   }
}


static bool load_mem_file(const char* const filename, file_mem_t &fm)
{
   fm.raw.clear();
   fm.data.clear();
   fm.enc = char_enc_t::ASCII;

   /* Grab the stat info for the file, return if it cannot be read */
   struct stat my_stat;
   retval_if(stat(filename, &my_stat) < 0, false)


#ifdef HAVE_UTIME_H
   /* Save off modification time */
   fm.utb.modtime = my_stat.st_mtime;
#endif

   /* Try to read in the file */
   FILE* p_file = fopen(filename, "rb");
   if(p_file == nullptr) { return (false); }
   retval_if(ptr_is_invalid(p_file), false);

   bool success = false;
   fm.raw.resize((uint32_t)my_stat.st_size);
   if (my_stat.st_size == 0) /* check if file is empty */
   {
      success = true;
      fm.bom  = false;
      fm.enc  = char_enc_t::ASCII;
      fm.data.clear();
   }
   else
   {
      /* read the raw data */
      if (fread(&fm.raw[0], fm.raw.size(), 1, p_file) != 1)
      {
         LOG_FMT(LERR, "%s: fread(%s) failed: %s (%d)\n",
                 __func__, filename, strerror(errno), errno);
         cpd.error_count++;
      }
      else if (!decode_unicode(fm.raw, fm.data, fm.enc, fm.bom))
      {
         LOG_FMT(LERR, "%s: failed to decode the file '%s'\n", __func__, filename);
         cpd.error_count++;
      }
      else
      {
         LOG_FMT(LNOTE, "%s: '%s' encoding looks like %s (%d)\n", __func__, filename,
               get_enc_name(fm.enc), fm.enc);
         success = true;
      }
   }
   fclose(p_file);
   return(success);
}


static bool load_mem_file_config(const char* const filename, file_mem_t &fm)
{
   char buf[1024];
   snprintf(buf, sizeof(buf), "%.*s%s", (int32_t)path_dirname_len(cpd.filename), cpd.filename, filename);

   bool success = load_mem_file(buf, fm);
   if (success == false)
   {
      success = load_mem_file(filename, fm);
      if (success == false)
      {
         LOG_FMT(LERR, "Failed to load (%s) or (%s)\n", buf, filename);
         cpd.error_count++;
      }
   }
   return(success);
}


static bool load_header_file(uo_t option, file_mem_t &file_mem)
{
   bool success = false;
   if ((cpd.settings[option].str    != nullptr) && /* option holds a string */
       (cpd.settings[option].str[0] != 0      ) )  /* that is not empty */
   {
      /* try to load the file referred to by the options string */
      success = load_mem_file_config(cpd.settings[option].str, file_mem);
   }
   return success;
}


bool load_all_header_files(void)
{
   bool success = false;
   /*lint -save -e514 */
   success |= load_header_file(UO_cmt_insert_file_header,   cpd.file_hdr  );
   success |= load_header_file(UO_cmt_insert_file_footer,   cpd.file_ftr  );
   success |= load_header_file(UO_cmt_insert_func_header,   cpd.func_hdr  );
   success |= load_header_file(UO_cmt_insert_class_header,  cpd.class_hdr );
   success |= load_header_file(UO_cmt_insert_oc_msg_header, cpd.oc_msg_hdr);
   /*lint -restore */
   return(success);
}


static const char* make_output_filename(char* buf, const uint32_t buf_size,
       const char* const filename, const char* const prefix, const char* const suffix)
{
   int32_t len = 0;

   /* if we got a prefix add it before the filename with a slash */
   if (ptr_is_valid(prefix))
   {
      len = snprintf(&buf[len], buf_size, "%s/", prefix);
   }


   snprintf(&buf[len], buf_size - (uint32_t)len, "%s%s", filename,
            (ptr_is_valid(suffix)) ? suffix : "");

   return(buf);
}


#define FILE_CHUNK_SIZE 1024
static bool file_content_matches(const string &filename1, const string &filename2)
{
   struct stat st1;
   struct stat st2;

   /* Check the file sizes first */
   if ((stat(filename1.c_str(), &st1) != 0) ||
       (stat(filename2.c_str(), &st2) != 0) ||
       (st1.st_size != st2.st_size))
   {
      return(false);
   }

   int32_t fd1, fd2;
   if((fd1 = open(filename1.c_str(), O_RDONLY)) < 0) {             return(false); }
   if((fd2 = open(filename2.c_str(), O_RDONLY)) < 0) { close(fd1); return(false); }

   uint32_t len1 = 0;
   uint32_t len2 = 0;
   uint8_t  buf1[FILE_CHUNK_SIZE];
   uint8_t  buf2[FILE_CHUNK_SIZE];
   memset(buf1, 0, sizeof(buf1));
   memset(buf2, 0, sizeof(buf2));
   while (true)
   {
      if (len1 == 0) { len1 = (uint32_t)read(fd1, buf1, sizeof(buf1)); }
      if (len2 == 0) { len2 = (uint32_t)read(fd2, buf2, sizeof(buf2)); }

      break_if((len1 == 0) || (len2 == 0)); /* reached end of either files */
      /* \todo what is if one file is longer than the other, do we miss that? */

      const uint32_t minlen = min(len1, len2);
      break_if(memcmp(buf1, buf2, minlen) != 0); /* found a difference */

      len1 -= minlen;
      len2 -= minlen;
   }
   close(fd1);
   close(fd2);
   return((len1 == 0) && (len2 == 0));
}


static string create_out_filename(const char* const filename)
{
   const char file_ending[]    = ".uncrustify";
   const uint32_t new_name_len = strlen(filename) + strlen(file_ending) + 1;
   char* const new_filename    = new char[new_name_len];
   if(new_filename == nullptr)
   {
      LOG_FMT(LERR, "Failed to allocate memory in %s \n", __func__);
   }
   else
   {
      sprintf(new_filename, "%s%s", filename, file_ending);
      string rv = new_filename;
      delete[] new_filename;
      return(rv);
   }
   /* \better change last letter or original termination to ensure input file
    * gets not overwritten */
   return(filename); /* return unchanged filename as error recovery */
}


static bool bout_content_matches(const file_mem_t &fm, const bool report_status)
{
   bool is_same = true;

   /* compare the old data vs the new data */
   if (cpd.bout->size() != fm.raw.size())
   {
      if (report_status)
      {
         fprintf(stderr, "FAIL: %s (File size changed from %u to %u)\n",
                 cpd.filename, (uint32_t)fm.raw.size(), (uint32_t)cpd.bout->size());
         log_flush(true);
      }
      is_same = false;
   }
   else
   {
      for (uint32_t idx = 0; idx < fm.raw.size(); idx++)
      {
         if (fm.raw[idx] != (*cpd.bout)[idx])
         {
            if (report_status)
            {
               fprintf(stderr, "FAIL: %s (Difference at byte %u)\n",
                       cpd.filename, idx);
               log_flush(true);
            }
            is_same = false;
            break;
         }
      }
   }
   if ((is_same       == true) &&
       (report_status == true) )
   {
      fprintf(stdout, "PASS: %s (%u bytes)\n", cpd.filename, (uint32_t)fm.raw.size());
   }

   return(is_same);
}


static void rename_file(const char* old_name, const char* new_name)
{
#ifdef WIN32
   /* Atomic rename in windows can't go through stdio rename() func because
    * underneath it calls MoveFileExW without MOVEFILE_REPLACE_EXISTING. */
   if (!MoveFileEx(old_name, new_name, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
#else
   if (rename(old_name, new_name) != 0)
#endif
   {
      LOG_FMT(LERR, "%s: Unable to rename '%s' to '%s'\n", __func__, old_name, new_name);
      cpd.error_count++;
   }
}


static void do_source_file(const char* filename_in, const char* filename_out,
      const char *parsed_file, bool no_backup, bool keep_mtime)
{
   UNUSED(keep_mtime);

   /* Do some simple language detection based on the filename extension */
   if ((cpd.lang_forced == false) || is_lang(LANG_NONE))
   {
      cpd.lang_flags = language_flags_from_filename(filename_in);
   }

   /* Try to read in the source file */
   file_mem_t fm;
   if (load_mem_file(filename_in, fm) == false)
   {
      LOG_FMT(LERR, "Failed to load (%s)\n", filename_in);
      cpd.error_count++;
      return;
   }

   LOG_FMT(LSYS, "Parsing: %s as language %s\n",
           filename_in, language_name_from_flags(cpd.lang_flags));

   cpd.filename = filename_in;

   /* If we're only going to write on an actual change, then build the output
    * buffer now and if there were changes, run it through the normal file
    * write path.
    *
    * \todo: many code paths could be simplified if 'bout' were always used
    * and not optionally selected in just for do_check and if_changed. */
   if (cpd.if_changed == true)
   {
      /* Cleanup is deferred because we need 'bout' preserved long enough
       * to write it to a file (if it changed). */
      uncrustify_file(fm, nullptr, parsed_file, true);
      if (bout_content_matches(fm, false))
      {
         uncrustify_end();
         return;
      }
   }

   string filename_tmp;
   bool   need_backup = false;   /* define if we want a backup copy */
   bool   did_open    = false;
   FILE*  pfout       = nullptr;
   if (cpd.do_check == false)
   {
      if (ptr_is_invalid(filename_out)) { pfout = stdout; }
      else
      {
         /* If the out file is the same as the in file, then use a temp file */
         filename_tmp = filename_out;
         if (strcmp(filename_in, filename_out) == 0)
         {
            filename_tmp = create_out_filename(filename_out);

            if (no_backup == false)
            {
               if (backup_copy_file(filename_in, fm.raw) != EX_OK)
               {
                  LOG_FMT(LERR, "%s: Failed to create backup file for %s\n",
                          __func__, filename_in);
                  cpd.error_count++;
                  return;
               }
               need_backup = true;
            }
         }
         make_folders(filename_tmp);

         pfout = fopen(filename_tmp.c_str(), "wb");
         if (ptr_is_invalid(pfout))
         {
            LOG_FMT(LERR, "%s: Unable to create %s: %s (%d)\n",
                    __func__, filename_tmp.c_str(), strerror(errno), errno);
            cpd.error_count++;
            return;
         }
         did_open = true;
         //LOG_FMT(LSYS, "Output file %s\n", filename_out);
      }
   }

   if (ptr_is_valid(pfout))
   {
      if (cpd.if_changed == true)
      {
         deque<uint8_t>::const_iterator i   = cpd.bout->begin();
         deque<uint8_t>::const_iterator end = cpd.bout->end();
         for (; i != end; ++i)
         {
            fputc(*i, pfout);
         }
         uncrustify_end();
      }
      else
      {
         uncrustify_file(fm, pfout, parsed_file);
      }
   }

   if (did_open == true)
   {
      if (ptr_is_valid(pfout)) { fclose(pfout);                       }
      if (need_backup == true) { backup_create_md5_file(filename_in); }

      if (filename_tmp != filename_out)
      {
         /* We need to compare and then do a rename (but avoid redundant test when if_changed set) */
         if ((cpd.if_changed == false                       ) &&
             file_content_matches(filename_tmp, filename_out) )
         {
            /* No change - remove tmp file */
            UNUSED(unlink(filename_tmp.c_str()));
         }
         else
         {
            rename_file(filename_tmp.c_str(), filename_out);
         }
      }

      if (keep_mtime == true)
      {
#ifdef HAVE_UTIME_H
         /* update mtime -- don't care if it fails */
         fm.utb.actime = time(nullptr);
         UNUSED(utime(filename_in, &fm.utb));
#endif
      }
   }
}


static void add_file_header(void)
{
   if (is_cmt(chunk_get_head()) == false)
   {
      /*TODO: detect the typical #ifndef FOO / #define FOO sequence */
      tokenize(cpd.file_hdr.data, chunk_get_head());
   }
}


static void add_file_footer(void)
{
   chunk_t* pc = chunk_get_tail();

   /* Back up if the file ends with a newline */
   if (is_nl(pc))
   {
      pc = chunk_get_prev(pc);
   }
   if ((!is_cmt(pc) || !is_nl(chunk_get_prev(pc))))
   {
      pc = chunk_get_tail();
      if (!is_nl(pc))
      {
         LOG_FMT(LSYS, "Adding a newline at the end of the file\n");
         newline_add_after(pc);
      }
      tokenize(cpd.file_ftr.data, nullptr);
   }
}


static void perform_insert(chunk_t* ref, const file_mem_t &fm)
{
   /* Insert between after and ref */
   chunk_t* after = get_next_ncnl(ref);
   tokenize(fm.data, after);
   for (chunk_t* tmp = chunk_get_next(ref); tmp != after; tmp = chunk_get_next(tmp))
   {
      assert(are_valid(tmp, after));
      tmp->level = after->level;
   }
}


// \todo DRY with add_msg_header
static void add_func_header(c_token_t type, const file_mem_t &fm)
{
   chunk_t* pc;
   bool     do_insert;

   for (pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnlnp(pc))
   {
      continue_if(not_type(pc, type));
      continue_if(is_flag(pc, PCF_IN_CLASS       ) &&
          (is_false(UO_cmt_insert_before_inlines)) );

      // Check for one liners for classes. Declarations only. Walk down the chunks.
      chunk_t* ref = pc;
      if(is_type_and_ptype(ref, CT_CLASS, CT_NONE))
      {
         ref = ref->next;
         if(is_type_and_ptype(ref, CT_TYPE, type))
         {
            ref = ref->next;
            continue_if(is_type_and_ptype(ref, CT_SEMICOLON, CT_NONE));
         }
      }

      // Check for one liners for functions. There'll be a closing brace w/o any newlines. Walk down the chunks.
      ref = pc;
      if(is_type_and_ptype(ref, CT_FUNC_DEF, CT_NONE))
      {
         int32_t found_brace = 0; // Set if a close brace is found before a newline
         while (not_type(ref, CT_NEWLINE))
         {
            ref = ref->next;
            if (is_type(ref, CT_BRACE_CLOSE))
            {
               found_brace = 1;
               break;
            }
         }
         continue_if(found_brace);
      }

      do_insert = false;

      /* On a function prototype or def. Back up to a close brace or
       * semicolon on the same level */
      ref = pc;
      while ((ref = chunk_get_prev(ref)) != nullptr)
      {
         /* Bail if we change level or find an access specifier colon */
         if((ref->level != pc->level) || is_type(ref, CT_PRIVATE_COLON))
         {
            do_insert = true;
            break;
         }

         /* If we hit an angle close, back up to the angle open */
         if (is_type(ref, CT_ANGLE_CLOSE))
         {
            ref = get_prev_type(ref, CT_ANGLE_OPEN, (int32_t)ref->level, scope_e::PREPROC);
            continue;
         }

         /* Bail if we hit a preprocessor and cmt_insert_before_preproc is false */
         if (is_preproc(ref))
         {
            chunk_t* tmp = get_prev_type(ref, CT_PREPROC, (int32_t)ref->level);
            if (is_ptype(tmp, CT_PP_IF))
            {
               tmp = get_prev_nnl(tmp);
               break_if(is_cmt(tmp) &&
                   (is_false(UO_cmt_insert_before_preproc)) );
            }
         }

         /* Ignore 'right' comments */
         break_if(is_cmt(ref) && is_nl(chunk_get_prev(ref)));

         if (is_level(ref, pc->level) &&
             (is_preproc(ref) || is_type(ref, CT_SEMICOLON, CT_BRACE_CLOSE)))
         {
            do_insert = true;
            break;
         }
      }

      if (do_insert) { perform_insert(ref, fm); }
   }
}


// \todo DRY with add_func_header
static void add_msg_header(c_token_t type, const file_mem_t &fm)
{
   chunk_t* pc;
   bool     do_insert;

   for (pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnlnp(pc))
   {
      continue_if(not_type(pc, type));

      do_insert = false;

      /* On a message declaration back up to a Objective-C scope
       * the same level */
      chunk_t *ref = pc;
      while ((ref = chunk_get_prev(ref)) != nullptr)
      {
         /* ignore the CT_TYPE token that is the result type */
         continue_if((ref->level != pc->level)  &&
             is_type(ref, CT_TYPE, CT_PTR_TYPE) );

         /* If we hit a parentheses around return type, back up to the open parentheses */
         if (is_type(ref, CT_PAREN_CLOSE))
         {
            ref = get_prev_type(ref, CT_PAREN_OPEN, (int32_t)ref->level, scope_e::PREPROC);
            continue;
         }

         /* Bail if we hit a preprocessor and cmt_insert_before_preproc is false */
         if (is_preproc(ref))
         {
            chunk_t *tmp = get_prev_type(ref, CT_PREPROC, (int32_t)ref->level);
            if (is_ptype(tmp, CT_PP_IF))
            {
               tmp = get_prev_nnl(tmp);
               break_if(is_cmt(tmp) && (is_false(UO_cmt_insert_before_preproc)));
            }
         }
         if (is_level(ref, pc->level) &&
            (is_preproc(ref) || is_type(ref, CT_OC_SCOPE)))
         {
            ref = chunk_get_prev(ref);
            if (is_valid(ref))
            {
               /* Ignore 'right' comments */
               break_if(is_nl(ref) && is_cmt(chunk_get_prev(ref)));
               do_insert = true;
            }
            break;
         }
      }

      if (do_insert) { perform_insert(ref, fm); }
   }
}


static void uncrustify_start(const deque<uint32_t> &data)
{
   /* the configuration and file type is fix here.
    * We can setup the function arrays that are
    * used during the checking and processing */
   init_examine_brace_check_array();
   init_space_check_action_array();
   init_vbrace_check_array();

   /* Parse the text into chunks */
   tokenize(data, nullptr);

   cpd.unc_stage = unc_stage_e::HEADER;

   /* Get the column for the fragment indent */
   if (cpd.frag)
   {
      const chunk_t* pc = chunk_get_head();
      cpd.frag_cols = (uint16_t)((is_valid(pc)) ? pc->orig_col : 0);
   }

   if (cpd.file_hdr.data.size() > 0) { add_file_header(); } /* Add the file header */
   if (cpd.file_ftr.data.size() > 0) { add_file_footer(); } /* Add the file footer */

   tokenize_cleanup();
   brace_cleanup();

   /* At this point, the level information is available and accurate. */
   if (is_lang(LANG_PAWN)) { pawn_prescan(); }

   fix_symbols();
   mark_comments();
   combine_labels();
}


static void set_nl_chunk_pos(uo_t check, uo_t set, c_token_t token)
{
   if (not_tok(cpd.settings[check].tp, TP_IGNORE))
   {
      nl_chunk_pos(token, cpd.settings[set].tp);
   }
}


void uncrustify_file(const file_mem_t& fm, FILE* pfout,
                     const char* parsed_file, bool defer_uncrustify_end)
{
   const deque<uint32_t> &data = fm.data;

   /* Save off the encoding and whether a BOM is required */
   cpd.bom = fm.bom;
   cpd.enc = fm.enc;
   if ( is_true(UO_utf8_force) ||
       (is_true(UO_utf8_byte ) && (cpd.enc == char_enc_t::BYTE)) )
   {
      cpd.enc = char_enc_t::UTF8;
   }
   argval_t av;
   switch (cpd.enc)
   {
      case char_enc_t::UTF8:      av = get_arg(UO_utf8_bom); break;
      case char_enc_t::UTF16_LE: /* fallthrough */
      case char_enc_t::UTF16_BE:  av = AV_FORCE;             break;
      default:                         av = AV_IGNORE;            break;
   }

   if      (av == AV_REMOVE) { cpd.bom = false; }
   else if (av != AV_IGNORE) { cpd.bom = true;  }

   /* Check for embedded 0's (represents a decoding failure or corrupt file) */
   for (uint32_t idx = 0; idx < data.size() - 1; idx++)
   {
      if (data[idx] == 0)
      {
         LOG_FMT(LERR, "An embedded 0 was found in '%s'.\n", cpd.filename);
         LOG_FMT(LERR, "The file may be encoded in an unsupported Unicode format.\n");
         LOG_FMT(LERR, "Aborting.\n");
         cpd.error_count++;
         return;
      }
   }

   uncrustify_start(data);

   cpd.unc_stage = unc_stage_e::OTHER;

   /* Done with detection. Do the rest only if the file will go somewhere.
    * The detection code needs as few changes as possible. */

   /* Add comments before function definitions and classes */
   if (cpd.func_hdr.data.empty() == false)
   {
      add_func_header(CT_FUNC_DEF, cpd.func_hdr);
      if (is_true(UO_cmt_insert_before_ctor_dtor))
      {
         add_func_header(CT_FUNC_CLASS_DEF, cpd.func_hdr);
      }
   }
   if (!cpd.class_hdr.data.empty() ) { add_func_header(CT_CLASS,       cpd.class_hdr ); }
   if (!cpd.oc_msg_hdr.data.empty()) { add_msg_header (CT_OC_MSG_DECL, cpd.oc_msg_hdr); }

   do_braces(); /* Change virtual braces into real braces... */

   if (is_true(UO_mod_remove_extra_semicolon)) { remove_extra_semicolons(); }
   if (is_true(UO_mod_remove_empty_return   )) { remove_extra_returns();    }
   do_parens();
   if (get_uval(UO_nl_remove_extra_newlines) == 2) { remove_newlines(); }

   bool first = true;
   int32_t  old_changes;
   cpd.pass_count = 3;
   do
   {
      old_changes = cpd.changes;

      LOG_FMT(LNEWLINE, "Newline loop start: %d\n", cpd.changes);

      annotations_nl();
      newlines_cleanup_dup();
      cleanup_braces(first);

      if (is_true(UO_nl_after_multiline_comment)) { nl_after_multiline_cmt(); }
      if (is_true(UO_nl_after_label_colon      )) { newline_after_label_colon();       }

      insert_blank_lines();

      set_nl_chunk_pos(UO_pos_bool,        UO_pos_bool,        CT_BOOL      );
      set_nl_chunk_pos(UO_pos_compare,     UO_pos_compare,     CT_COMPARE   );
      set_nl_chunk_pos(UO_pos_conditional, UO_pos_conditional, CT_COND_COLON);
      set_nl_chunk_pos(UO_pos_conditional, UO_pos_conditional, CT_QUESTION  );
      set_nl_chunk_pos(UO_pos_comma,       UO_pos_comma,       CT_COMMA     );
      set_nl_chunk_pos(UO_pos_enum_comma,  UO_pos_comma,       CT_COMMA     );
      set_nl_chunk_pos(UO_pos_assign,      UO_pos_assign,      CT_ASSIGN    );
      set_nl_chunk_pos(UO_pos_arith,       UO_pos_arith,       CT_ARITH     );
      set_nl_chunk_pos(UO_pos_arith,       UO_pos_arith,       CT_CARET     );

      nl_class_colon_pos(CT_CLASS_COLON );
      nl_class_colon_pos(CT_CONSTR_COLON);

      if (is_true(UO_nl_squeeze_ifdef)) { newlines_squeeze_ifdef(); }

      do_blank_lines();
      nl_eat_start_and_end();
      newlines_functions_remove_extra_blank_lines();
      newlines_cleanup_dup();
      first = false;
      cpd.pass_count--;
   } while ((old_changes    != cpd.changes) &&
            (cpd.pass_count !=   0        ) );

   mark_comments();

   /* Add balanced spaces around nested params */
   if (is_true(UO_sp_bal_nested_parens))  { space_text_balance_nested_parens(); }

   /* Scrub certain added semicolons */
   if (is_lang(LANG_PAWN) &&
       (is_true(UO_mod_pawn_semicolon)) ) { pawn_scrub_vsemi(); }

   /* Sort imports/using/include */
   if (is_true(UO_mod_sort_import ) ||
       is_true(UO_mod_sort_include) ||
       is_true(UO_mod_sort_using  ) ) { sort_imports(); }

   /* Fix same-line inter-chunk spacing */
   space_text();

   /* Do any aligning of preprocessors */
   if (get_uval(UO_align_pp_define_span) > 0) { align_preprocessor(); }

   /* Indent the text */
   indent_preproc();
   indent_text();

   /* Insert trailing comments after certain close braces */
   if ((get_uval(UO_mod_add_long_switch_closebrace_comment   ) > 0) ||
       (get_uval(UO_mod_add_long_function_closebrace_comment ) > 0) ||
       (get_uval(UO_mod_add_long_class_closebrace_comment    ) > 0) ||
       (get_uval(UO_mod_add_long_namespace_closebrace_comment) > 0) )
   {
      add_long_closebrace_comment();
   }

   /* Insert trailing comments after certain preprocessor conditional blocks */
   if ((get_uval(UO_mod_add_long_ifdef_else_comment ) > 0) ||
       (get_uval(UO_mod_add_long_ifdef_endif_comment) > 0) )
   {
      add_long_pp_conditional_block_cmt();
   }

   /* Align everything else, reindent and break at code_width */
   first          = true;
   cpd.pass_count = 3;
   do
   {
      align_all();
      indent_text();
      old_changes = cpd.changes;
      if (get_uval(UO_code_width) > 0)
      {
         LOG_FMT(LNEWLINE, "Code_width loop start: %d\n", cpd.changes);
         do_code_width();
         if ((old_changes != cpd.changes) &&
             (first       == true       ) )
         {
            /* retry line breaks caused by splitting 1-liners */
            cleanup_braces(false);
            insert_blank_lines();
            first = false;
         }
      }
   } while ((old_changes      != cpd.changes) &&
            (cpd.pass_count--  > 0          ) );

   /* And finally, align the backslash newline stuff */
   align_right_comments();
   if (is_true(UO_align_nl_cont)) { align_backslash_newline(); }

   /* Now render it all to the output file */
   output_text(pfout);

   /* Special hook for dumping parsed data for debugging */
   if (ptr_is_valid(parsed_file))
   {
      FILE *p_file = fopen(parsed_file, "w");
      if (ptr_is_valid(p_file))
      {
         output_parsed(p_file);
         fclose(p_file);
      }
      else
      {
         LOG_FMT(LERR, "%s: Failed to open '%s' for write: %s (%d)\n",
                 __func__, parsed_file, strerror(errno), errno);
         cpd.error_count++;
      }
   }

   if ( (cpd.do_check                           ) &&
        (bout_content_matches(fm, true) == false) )  { cpd.check_fail_cnt++; }
   if (  defer_uncrustify_end           == false  )  { uncrustify_end();     }
}


void uncrustify_end(void)
{
   /* Free all the memory */
   cpd.unc_stage = unc_stage_e::CLEANUP;

   chunk_t* pc;
   while ( (pc = chunk_get_head()) != nullptr)
   {
      chunk_del(pc);
   }

   if (cpd.bout) { cpd.bout->clear(); }

   /* Clean up some state variables */
   cpd.unc_off     = false;
   cpd.al_cnt      = 0;
   cpd.did_newline = true;
   cpd.frame_count = 0;
   cpd.pp_level    = 0;
   cpd.changes     = 0;
   cpd.is_preproc  = CT_NONE;
   cpd.consumed    = false;
   memset(cpd.le_counts, 0, sizeof(cpd.le_counts));
   cpd.preproc_ncnl_count                     = 0;
   cpd.ifdef_over_whole_file                  = 0;
   cpd.warned_unable_string_replace_tab_chars = false;
}


bool is_valid_token_name(c_token_t token)
{
   return ( ((uint32_t)token < ARRAY_SIZE(token_names)) &&
             ptr_is_valid(token_names[token]        ) );
}


const char* get_token_name(c_token_t token)
{
   return (is_valid_token_name(token) ? token_names[token] : "unknown");
}


bool is_nonempty_string(const char* str)
{
   return (ptr_is_valid(str) &&  /* pointer is not null */
          (*str != 0       ) );  /* first character is no termination character */
}


c_token_t find_token_name(const char* text)
{
   if (is_nonempty_string(text))
   {
      for (int32_t idx = 1; idx < static_cast<int32_t> ARRAY_SIZE(token_names); idx++)
      {
         if (strcasecmp(text, token_names[idx]) == 0)
         {
            return(static_cast<c_token_t>(idx));
         }
      }
   }
   return(CT_NONE);
}


static bool ends_with(const char* filename, const char* tag, bool case_sensitive = true)
{
   uint32_t len1 = strlen(filename);
   uint32_t len2 = strlen(tag     );

   return((len2 <= len1) &&
          (((case_sensitive == true ) && (strcmp    (&filename[len1 - len2], tag) == 0)) ||
           ((case_sensitive == false) && (strcasecmp(&filename[len1 - len2], tag) == 0)) ));
}


static lang_t language_flags_from_name(const char *name)
{
   for (const auto &language : language_names)
   {
      if (strcasecmp(name, language.name) == 0)
      {
         return(language.lang);
      }
   }
   return(LANG_NONE);
}


const char* language_name_from_flags(uint32_t lang)
{
   /* Check for an exact match first */
   for (auto &language_name : language_names)
   {
      if (language_name.lang == lang)
      {
         return(language_name.name);
      }
   }

   /* Check for the first set language bit */
   for (auto &language_name : language_names)
   {
      if ((language_name.lang & lang) != 0)
      {
         return(language_name.name);
      }
   }
   return("unknown");
}


const char* get_file_extension(uint32_t &idx)
{
   const char* val = nullptr;

   if (idx < ARRAY_SIZE(language_exts))
   {
      val = language_exts[idx].ext;
   }
   idx++;
   return(val);
}


// maps a file extension to a language flag. include the ".", as in ".c".
// These ARE case sensitive user file extensions.
typedef std::map<string, string> extension_map_t;
static extension_map_t g_ext_map;


const char* extension_add(const char* ext_text, const char* lang_text)
{
   uint32_t lang_flags = language_flags_from_name(lang_text);
   if (lang_flags != 0)
   {
      const char* lang_name = language_name_from_flags(lang_flags);
      g_ext_map[string(ext_text)] = lang_name;
      return(lang_name);
   }
   else
   {
      return(nullptr);
   }
}


void print_extensions(FILE* pfile)
{
   for (auto &language : language_names)
   {
      bool did_one = false;
      for (auto &extension_val : g_ext_map)
      {
         if (strcmp(extension_val.second.c_str(), language.name) == 0)
         {
            if (did_one == false)
            {
               fprintf(pfile, "file_ext %s", extension_val.second.c_str());
               did_one = true;
            }
            fprintf(pfile, " %s", extension_val.first.c_str());
         }
      }

      if (did_one) { fprintf(pfile, "\n"); }
   }
}


/* \todo better use enum lang_t for source file language */
static lang_t language_flags_from_filename(const char* filename)
{
   /* check custom extensions first */
   for (const auto &extension_val : g_ext_map)
   {
      if (ends_with(filename, extension_val.first.c_str()))
      {
         return(language_flags_from_name(extension_val.second.c_str()));
      }
   }

   for (auto &lanugage : language_exts)
   {
      if (ends_with(filename, lanugage.ext))
      {
         return(language_flags_from_name(lanugage.name));
      }
   }

   /* check again without case sensitivity */
   for (auto &extension_val : g_ext_map)
   {
      if (ends_with(filename, extension_val.first.c_str(), false))
      {
         return(language_flags_from_name(extension_val.second.c_str()));
      }
   }

   for (auto &lanugage : language_exts)
   {
      if (ends_with(filename, lanugage.ext, false))
      {
         return(language_flags_from_name(lanugage.name));
      }
   }
   return(LANG_C);
}


void log_pcf_flags(log_sev_t sev, uint64_t flags)
{
   return_if(log_sev_on(sev) == false);

   log_fmt(sev, "[0x%" PRIx64 ":", flags);

   const char* tolog = nullptr;
   for (uint32_t i = 0; i < ARRAY_SIZE(pcf_names); i++)
   {
      if (flags & (1ULL << i))
      {
         if (ptr_is_valid(tolog))
         {
            log_str(sev, tolog, strlen(tolog));
            log_str(sev, ",", 1);
         }
         tolog = pcf_names[i];
      }
   }

   if (ptr_is_valid(tolog)) { log_str(sev, tolog, strlen(tolog)); }
   log_str(sev, "]\n", 2);
}
