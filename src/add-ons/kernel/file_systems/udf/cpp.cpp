/* cpp - C++ in the kernel
**
** Initial version by Axel Dörfler, axeld@pinc-software.de
** This file may be used under the terms of the OpenBeOS License.
*/


#include "cpp.h"


extern "C" void
__pure_virtual()
{
	//printf("pure virtual function call");
}

const nothrow_t nothrow = {};
