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
 * \file 		token.cpp
 * \note		   Project: uncrustify
 * \date		   30.01.2017
 * \author		Stefan Nunninger
 * \author		nuinno, Erlangen
 ******************************************************************************/

/** @addtogroup common
 *  @{
*/


/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "token_enum.h"


/*******************************************************************************
 * Private Defines, Typedefs, Enums, etc.
 ******************************************************************************/


/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/


/*******************************************************************************
 * Global variables
 ******************************************************************************/


/*******************************************************************************
 * Function Implementation
 ******************************************************************************/

c_token_t get_inverse_type(c_token_t type)
{
   switch(type)
   {
      case(CT_PAREN_OPEN   ): return (CT_PAREN_CLOSE  );
      case(CT_PAREN_CLOSE  ): return (CT_PAREN_OPEN   );

      case(CT_ANGLE_OPEN   ): return (CT_ANGLE_CLOSE  );
      case(CT_ANGLE_CLOSE  ): return (CT_ANGLE_OPEN   );

      case(CT_SPAREN_OPEN  ): return (CT_SPAREN_CLOSE );
      case(CT_SPAREN_CLOSE ): return (CT_SPAREN_OPEN  );

      case(CT_FPAREN_OPEN  ): return (CT_FPAREN_CLOSE );
      case(CT_FPAREN_CLOSE ): return (CT_FPAREN_OPEN  );

      case(CT_TPAREN_OPEN  ): return (CT_TPAREN_CLOSE );
      case(CT_TPAREN_CLOSE ): return (CT_TPAREN_OPEN  );

      case(CT_BRACE_OPEN   ): return (CT_BRACE_CLOSE  );
      case(CT_BRACE_CLOSE  ): return (CT_BRACE_OPEN   );

      case(CT_VBRACE_OPEN  ): return (CT_VBRACE_CLOSE );
      case(CT_VBRACE_CLOSE ): return (CT_VBRACE_OPEN  );

      case(CT_SQUARE_OPEN  ): return (CT_SQUARE_CLOSE );
      case(CT_SQUARE_CLOSE ): return (CT_SQUARE_OPEN  );

      case(CT_SQL_BEGIN    ): return (CT_SQL_END      );
      case(CT_SQL_END      ): return (CT_SQL_BEGIN    );

      case(CT_MACRO_OPEN   ): return (CT_MACRO_CLOSE  );
      case(CT_MACRO_CLOSE  ): return (CT_MACRO_OPEN   );

      case(CT_VSEMICOLON   ): return (CT_SEMICOLON    );
      case(CT_SEMICOLON    ): return (CT_VSEMICOLON   );

      case(CT_INCDEC_BEFORE): return (CT_INCDEC_AFTER );
      case(CT_INCDEC_AFTER ): return (CT_INCDEC_BEFORE);

      case(CT_COMMENT_START): return (CT_COMMENT_END  );
      case(CT_COMMENT_END  ): return (CT_COMMENT_START);

      case(CT_PLUS         ): return (CT_MINUS        );
      case(CT_MINUS        ): return (CT_PLUS         );

      case(CT_POS          ): return (CT_NEG          );
      case(CT_NEG          ): return (CT_POS          );

      default:                return type;
   }
}


/** @} */
