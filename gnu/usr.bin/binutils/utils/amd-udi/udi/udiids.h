/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * Comments about this software should be directed to udi@amd.com. If access
 * to electronic mail isn't available, send mail to:
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 *****************************************************************************
 *       $Id: @(#)udiids.h	3.2, AMD
 */

/*  This file contains the DFE and TIP IDs to be used by AMD products for */
/*  the UDICapabilities call                                              */

	/* Company Codes -- AMD assigns these */
#define UDICompanyCode_AMD 1
#define UDICompanyCode_Honeywell 2
#define UDICompanyCode_EPI 3

	/* Build a UDIID given a CompanyProdCode and 3 version pieces */
#define UDIID(CompanyProdCode, v1,v2,v3) ((((CompanyProdCode) & 0xfffff)<<12)+\
				  (((v1)&0xf)<<8) + (((v2)&0xf)<<4) + ((v3)&0xf)) 


	/* Extract a CompanyProdCode or a Version from a UDIID */
#define UDIID_CompanyProdCode(id) (((id)>>12) & 0xfffff)
#define UDIID_Version(id) ((id)&0xfff)


#define UDIAMDProduct(ProdCode) ((UDICompanyCode_AMD<<4) + (ProdCode&0xf))

	/* AMD DFE Product Codes */
#define UDIProductCode_Mondfe UDIAMDProduct(0)
#define UDIProductCode_XRAY   UDIAMDProduct(1)
#define UDIProductCode_TIPTester  UDIAMDProduct(2)

	/* AMD TIP Product Codes (need not be distinct from DFE Product Codes) */
#define UDIProductCode_Montip UDIAMDProduct(0)
#define UDIProductCode_Isstip UDIAMDProduct(1)
#define UDIProductCode_MON29Ktip UDIAMDProduct(2)

#ifdef UDI13
#define UDILatestVersion 0x130  /* UDI 1.3.0, can be used in DFE and TIP desired UDI params */
#else
#define UDILatestVersion 0x120  /* UDI 1.2.0, can be used in DFE and TIP desired UDI params */
#endif /* UDI13 */
