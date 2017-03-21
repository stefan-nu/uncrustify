/**
 * @file logger.h
 *
 * Functions to do logging.
 * The macros check whether the logsev is active before evaluating the
 * parameters.  Use them instead of the functions.
 *
 * If a log statement ends in a newline, the current log is ended.
 * When the log severity changes, an implicit newline is inserted.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include "logmask.h"
#include <cstring>
#include <cstdio>


/**
 * Initializes the log subsystem - call this first.
 * This function sets the log stream and enables the top 3 sevs (0-2).
 */
void log_init(
   FILE* log_file /**< [in] nullptr for stderr or the FILE stream for logs */
);


/**
 * Show or hide the severity prefix "<1>"
 */
void log_show_sev(
   bool show /**< [in] true=show  false=hide */
);


/**
 * Returns whether a log severity is active.
 *
 * @return true/false
 */
bool log_sev_on(
   log_sev_t sev /**< [in] severity log level */
);


/**
 * Sets a log sev on or off
 *
 * @return true/false
 */
void log_set_sev(
   log_sev_t sev, /**< [in] severity log level to modify */
   bool      val  /**< [in] new value for severity log level */
);


/**
 * Sets the log mask
 */
void log_set_mask(
   const log_mask_t &mask /**< [in] The mask to copy */
);


/**
 * Gets the log mask
 */
void log_get_mask(
   log_mask_t &mask /**< [in] Where to copy the mask */
);


/**
 * Logs a string of known length
 */
void log_str(
   log_sev_t   sev, /**< [in] severity */
   const char* str, /**< [in] pointer to the string */
   uint32_t    len  /**< [in] length of the string from strlen(str) */
);


#define LOG_STR(sev, str, len)                           \
   do { if (log_sev_on(sev)) { log_str(sev, str, len); } \
   } while (0)

#define LOG_STRING(sev, str)                                     \
   do { if (log_sev_on(sev)) { log_str(sev, str, strlen(str)); } \
   } while (0)


/**
 * Logs a formatted string -- similar to printf()
 */
void log_fmt(
   log_sev_t   sev, /**< [in] severity */
   const char* fmt, /**< [in] format string */
   ...              /**< [in] Additional arguments */
)
__attribute__((format(printf, 2, 3)));

#ifdef NO_MACRO_VARARG
#define LOG_FMT    log_fmt
// \todo during debugging add source file and line number
#else
#define LOG_FMT(sev, args...)                           \
   do { if (log_sev_on(sev)) { log_fmt(sev, ## args); } \
   } while (0)
#endif


/**
 * Dumps hex characters inline, no newlines inserted
 */
void log_hex(
   log_sev_t   sev,   /**< [in] severity */
   const void* vdata, /**< [in] data to log */
   uint32_t    len    /**< [in] number of bytes to log */
);


#define LOG_HEX(sev, ptr, len)                           \
   do { if (log_sev_on(sev)) { log_hex(sev, ptr, len); } \
   } while (0)


/**
 * Logs a block of data in a pretty hex format
 * Numbers on the left, characters on the right, just like I like it.
 *
 * "nnn | XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX | ................"
 *  0     ^6                                            54^ ^56           72^
 *
 *  nnn is the line number or index/16
 */
void log_hex_blk(
   log_sev_t   sev,  /**< [in] severity */
   const void* data, /**< [in] data to log */
   uint32_t    len   /**< [in] number of bytes to log */
);


#define LOG_HEX_BLK(sev, ptr, len)                           \
   do { if (log_sev_on(sev)) { log_hex_blk(sev, ptr, len); } \
   } while (0)


/**
 * Returns the HEX digit for a low nibble in a number
 *
 * @param nibble  The nibble
 * @return        '0', '1', '2', '3', '4', '5', '6', '7',
 *                '8', '9', 'a', 'b', 'c', 'd', 'e', or 'f'
 */
static inline char to_hex_char(int32_t nibble)
{
   const char *hex_string = "0123456789abcdef";
   return(hex_string[nibble & 0x0F]);
}


#ifdef DEBUG

/**
 * This should be called as the first thing in a function.
 * It uses the log_func class to add an entry to the function log stack.
 * It is automatically removed when the function returns.
 */
#define LOG_FUNC_ENTRY()    log_func log_fe = log_func(__func__, __LINE__)

/**
 * This should be called right before a repeated function call to trace where
 * the function was called. It does not add an entry, but rather updates the
 * line number of the top entry.
 */
#define LOG_FUNC_CALL()     log_func_call(__LINE__)

#else
#define LOG_FUNC_ENTRY()
#define LOG_FUNC_CALL()
#endif

/**
 * This class just adds a entry to the top of the stack on construction and
 * removes it on destruction.
 * RAII for the win.
 */
class log_func
{
public:
   log_func(const char *name, int32_t line);
   ~log_func(); /**< [in]  */
};


/**
 * tbd
 */
void log_func_call(
   int32_t line /**< [in]  */
);


/**
 * tbd
 */
void log_func_stack(
   log_sev_t   sev,           /**< [in]  */
   const char* prefix = "",   /**< [in]  */
   const char* suffix = "\n", /**< [in]  */
   uint32_t      skip_cnt = 0   /**< [in]  */
);


#define log_func_stack_inline(_sev)    log_func_stack((_sev), " [CallStack:", "]\n", 1)


#endif /* LOGGER_H_INCLUDED */
