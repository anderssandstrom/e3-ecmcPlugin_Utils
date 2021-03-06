/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcPvaWrap.h
*
*  Created on: Mar 22, 2020
*      Author: anderssandstrom
*
\*************************************************************************/
#ifndef ECMC_PVA_WRAP_H_
#define ECMC_PVA_WRAP_H_

#include "ecmcPvDefs.h"

# ifdef __cplusplus
extern "C" {
# endif  // ifdef __cplusplus

  int    initPvs();
  int    parseConfigStr(char *configStr);
  void*  getPvRegObj();
  int    exePutDataCmd(int handle, double data);
  double getLastValue(int handle);
  int    getBusy(int handle);
  int    getConnected(int handle);  
  int    getError(int handle);
  void   cleanup();

# ifdef __cplusplus
}
# endif  // ifdef __cplusplus

#endif  /* ECMC_PVA_WRAP_H_ */
