/*******************************************************************************
 * Copyright (c) 2017 nuinno
 * Alle Rechte vorbehalten. All Rights Reserved.
 *
 * Information contained herein is subject to change without notice.
 * nuinno retains ownership and all other rights in the software and each
 * component thereof.
 * Any reproduction of the software or components thereof without the prior
 * written permission of nuinno is prohibited.
 ***************************************************************************//**
 * \file 		enum_types.h
 * \brief		header for enum_types.c
 * \details		provides all public definitions used in enum_types.c
 * \note		Project:  uncrustify
 * \note		Platform: Raspberry Pi 2 or 3 based on ARM Cortex-A
 * \note		Compiler: GCC
 * \author		stefan
 * \author		nuinno, Erlangen
 ******************************************************************************/

#ifndef INCLUDE_ENUM_TYPES_H_
#define INCLUDE_ENUM_TYPES_H_

/** @addtogroup common
 @{
*/

/*******************************************************************************
 * Includes
 ******************************************************************************/


/*******************************************************************************
 * Defines, Typedefs
 ******************************************************************************/
typedef enum StarStyle_e
{
   SS_IGNORE,   /**< don't look for prev stars */
   SS_INCLUDE,  /**< include prev * before add */
   SS_DANGLE    /**< include prev * after  add */
}StarStyle_t;


/*******************************************************************************
 * Function Prototyps
 ******************************************************************************/


/** @} */

#endif /* INCLUDE_ENUM_TYPES_H_ */
