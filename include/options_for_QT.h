/**
 * @file options_for_QT.h
 * Save the options which are needed to be changed to
 * process the SIGNAL and SLOT QT macros.
 * http://doc.qt.io/qt-4.8/qtglobal.html
 *
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          January 2016
 * @license GPL v2+
 */

#ifndef OPTIONS_FOR_QT_H_INCLUDED
#define OPTIONS_FOR_QT_H_INCLUDED

#include "uncrustify_types.h"


/**
 * tbd
 */
void save_set_options_for_QT(
   uint32_t level /**< [in]  */
);


/**
 * tbd
 */
void restore_options_for_QT(void);


/* \todo can we avoid those extern variables */
extern bool     QT_SIGNAL_SLOT_found;
extern uint32_t QT_SIGNAL_SLOT_level;
extern bool     restoreValues;


#endif /* OPTIONS_FOR_QT_H_INCLUDED */
