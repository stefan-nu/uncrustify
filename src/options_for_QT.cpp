/**
 * @file options_for_QT.cpp
 * Save the options which are needed to be changed to
 * process the SIGNAL and SLOT QT macros.
 * http://doc.qt.io/qt-4.8/qtglobal.html
 *
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */

#include "options_for_QT.h"

// for the modification of options within the SIGNAL/SLOT call.
bool     QT_SIGNAL_SLOT_found      = false;
uint32_t QT_SIGNAL_SLOT_level      = 0;
bool     restoreValues             = false;
argval_t SaveUO_sp_inside_fparen_A = AV_NOT_DEFINED;

// connect( timer,SIGNAL( timeout() ),this,SLOT( timeoutImage() ) );
argval_t SaveUO_sp_inside_fparens_A = AV_NOT_DEFINED;
argval_t SaveUO_sp_paren_paren_A    = AV_NOT_DEFINED;
argval_t SaveUO_sp_before_comma_A   = AV_NOT_DEFINED;
argval_t SaveUO_sp_after_comma_A    = AV_NOT_DEFINED;

// connect(&mapper, SIGNAL(mapped(QString &)), this, SLOT(onSomeEvent(QString &)));
argval_t SaveUO_sp_before_byref_A         = AV_NOT_DEFINED;
argval_t SaveUO_sp_before_unnamed_byref_A = AV_NOT_DEFINED;
argval_t SaveUO_sp_after_type_A           = AV_NOT_DEFINED;


void save_set_options_for_QT(uint32_t level)
{
   assert(is_true(UO_use_options_overriding_for_qt_macros));
   LOG_FMT(LGUY, "save values, level=%u\n", level);

   /* save the values */
   QT_SIGNAL_SLOT_level             = level;
   SaveUO_sp_inside_fparen_A        = get_arg(UO_sp_inside_fparen);
   SaveUO_sp_inside_fparens_A       = get_arg(UO_sp_inside_fparens);
   SaveUO_sp_paren_paren_A          = get_arg(UO_sp_paren_paren);
   SaveUO_sp_before_comma_A         = get_arg(UO_sp_before_comma);
   SaveUO_sp_after_comma_A          = get_arg(UO_sp_after_comma);
   SaveUO_sp_before_byref_A         = get_arg(UO_sp_before_byref);
   SaveUO_sp_before_unnamed_byref_A = get_arg(UO_sp_before_unnamed_byref);
   SaveUO_sp_after_type_A           = get_arg(UO_sp_after_type);
   /* set values for SIGNAL/SLOT */
   set_arg(UO_sp_inside_fparen,        AV_REMOVE);
   set_arg(UO_sp_inside_fparens,       AV_REMOVE);
   set_arg(UO_sp_paren_paren,          AV_REMOVE);
   set_arg(UO_sp_before_comma,         AV_REMOVE);
   set_arg(UO_sp_after_comma,          AV_REMOVE);
   set_arg(UO_sp_before_byref,         AV_REMOVE);
   set_arg(UO_sp_before_unnamed_byref, AV_REMOVE);
   set_arg(UO_sp_after_type,           AV_REMOVE);
   QT_SIGNAL_SLOT_found = true;
}


void restore_options_for_QT(void)
{
   assert(is_true(UO_use_options_overriding_for_qt_macros));
   LOG_FMT(LGUY, "restore values\n");
   /* restore the values we had before SIGNAL/SLOT */

   QT_SIGNAL_SLOT_level = 0;
   set_arg(UO_sp_inside_fparen,        SaveUO_sp_inside_fparen_A       );
   set_arg(UO_sp_inside_fparens,       SaveUO_sp_inside_fparens_A      );
   set_arg(UO_sp_paren_paren,          SaveUO_sp_paren_paren_A         );
   set_arg(UO_sp_before_comma,         SaveUO_sp_before_comma_A        );
   set_arg(UO_sp_after_comma,          SaveUO_sp_after_comma_A         );
   set_arg(UO_sp_before_byref,         SaveUO_sp_before_byref_A        );
   set_arg(UO_sp_before_unnamed_byref, SaveUO_sp_before_unnamed_byref_A);
   set_arg(UO_sp_after_type,           SaveUO_sp_after_type_A          );
   SaveUO_sp_inside_fparen_A        = AV_NOT_DEFINED;
   SaveUO_sp_inside_fparens_A       = AV_NOT_DEFINED;
   SaveUO_sp_paren_paren_A          = AV_NOT_DEFINED;
   SaveUO_sp_before_comma_A         = AV_NOT_DEFINED;
   SaveUO_sp_after_comma_A          = AV_NOT_DEFINED;
   SaveUO_sp_before_byref_A         = AV_NOT_DEFINED;
   SaveUO_sp_before_unnamed_byref_A = AV_NOT_DEFINED;
   SaveUO_sp_after_type_A           = AV_NOT_DEFINED;
   QT_SIGNAL_SLOT_found             = false;
   restoreValues                    = false;
}
