/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcPv.cpp
*
*  Created on: May 12, 2020
*      Author: anderssandstrom
*
\*************************************************************************/
#include "ecmcPv.h"
#include <sstream>

// Start worker threads for each object
void f_cmd_exe(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: Error: Worker thread PV object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }

  ecmcPv * pvObj = (ecmcPv*)obj;
  pvObj->exeCmdThread();
}

// Start monitor thread
void f_monitor(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: Error: Worker thread PV object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }

  ecmcPv * pvObj = (ecmcPv*)obj;
  pvObj->monitorThread();
}

ecmcPv::ecmcPv(std::string pvName,std::string providerName, int index) {
  name_            = pvName;
  providerName_    = providerName;
  errorCode_       = 0;
  destructs_       = 0;
  cmd_             = ECMC_PV_CMD_NONE;
  //ATOMIC_FLAG_INIT(busyLock_);
  busyLock_.test_and_set();
  index_           = index;
  valueLatestRead_ = 0;
  valueToWrite_    = 0;
  type_            = scalar;
  connected_       = false;

  // Create worker thread
  std::string threadname = "ecmc.cmd.pv"  + to_string(index_);
  cmdExeThread_ = epicsThreadCreate(threadname.c_str(), 0, 32768, f_cmd_exe, this);
  if( cmdExeThread_ == NULL) {
    throw std::runtime_error("Error: Failed cmd exe worker thread.");
  } 

  // Create monitor thread
  threadname = "ecmc.mon.pv" + to_string(index_);
  monThread_ = epicsThreadCreate(threadname.c_str(), 0, 32768, f_monitor, this);
  if( monThread_ == NULL) {
    throw std::runtime_error("Error: Failed create monitor worker thread.");
  }
}

ecmcPv::~ecmcPv() {
  doCmdEvent_.signal();
  destructs_ = 1;
  epicsThreadMustJoin(cmdExeThread_);
}

std::string ecmcPv::getPvName(){
  return name_;
}

std::string ecmcPv::getProvider() {
  return providerName_;
}

int ecmcPv::getError() {
  return errorCode_;
}

int ecmcPv::reset() {
  errorCode_ = 0;
  return 0;
}

void ecmcPv::getCmd() {

  if(errorCode_ == ECMC_PV_GET_ERROR || errorCode_ == ECMC_PV_BUSY) {
    reset(); // reset if try again
  }

  if (getEcmcEpicsIOCState()!=ECMC_IOC_STARTED_STATE) {
    errorCode_ = ECMC_PV_IOC_NOT_STARTED;
    throw std::runtime_error("Error: ECMC IOC not started.");
  }

  if(busyLock_.test_and_set()) {
    errorCode_ = ECMC_PV_BUSY;
    throw std::runtime_error("Error: Object busy. Get operation to "+ name_ + ") failed." );
  }  
  cmd_ =  ECMC_PV_CMD_GET;
  doCmdEvent_.signal();  
}

double ecmcPv::getLastReadValue() {
  return valueLatestRead_;
}

// Send Reg cmd to worker
void ecmcPv::regCmd() {
  if(busyLock_.test_and_set()) {
    errorCode_ = ECMC_PV_BUSY;
    throw std::runtime_error("Error: Object busy. Reg operation to "+ name_ + ") failed." );
  }  
  cmd_ =  ECMC_PV_CMD_REG;
  doCmdEvent_.signal();  
}

void ecmcPv::putCmd(double value) {

  if(errorCode_ == ECMC_PV_PUT_ERROR || errorCode_ == ECMC_PV_BUSY) {
    reset(); // reset if try again
  }
  
  if (getEcmcEpicsIOCState()!=ECMC_IOC_STARTED_STATE) {
    errorCode_ = ECMC_PV_IOC_NOT_STARTED;
    throw std::runtime_error("Error: ECMC IOC not started.");
  }

  if(busyLock_.test_and_set()) {
    errorCode_ = ECMC_PV_BUSY;
    throw std::runtime_error("Error: Object busy. Put operation to "+ name_ + ") failed." );
  }  
  
  cmd_ =  ECMC_PV_CMD_PUT;
  valueToWrite_ = value;

  // Execute cmd
  doCmdEvent_.signal();

  return;
}

bool ecmcPv::busy() {
  if(busyLock_.test_and_set()){
    return true;
  }
  else
  {
    busyLock_.clear();
    return false;
  }  
}

bool ecmcPv::connected() {
  return connected_;
}

void ecmcPv::exeCmdThread() {
  std::cerr << "Registering PV for: " << name_ << "\n";
  // Retry untill success..
  while(not connected_) {
    try{
      connect();      
    }
    catch(std::exception &e){
      std::cerr << "Error: Connect failed: " << e.what() << "Try to reconnect in " 
                << to_string(ECMC_PV_TIME_BETWEEN_RECONNECT) << "s\n";
      errorCode_ = ECMC_PV_REG_ERROR;
      epicsThreadSleep(ECMC_PV_TIME_BETWEEN_RECONNECT);
    }
  }

  // Now connected
  while(true) {
    busyLock_.clear();
    doCmdEvent_.wait();
    reset();
    if(destructs_) {
      break; 
    }

    switch(cmd_) {
      case ECMC_PV_CMD_GET:
        try{
          if(connected_) {
            valueLatestRead_=getDouble();
          }else {
            errorCode_ = ECMC_PV_REG_ERROR;
          }
        }
        catch(std::exception &e){
          errorCode_ = ECMC_PV_GET_ERROR;
        }
        break;
      case ECMC_PV_CMD_PUT:
        try{
          if(connected_) {
            putDouble(valueToWrite_);
          }else {
            errorCode_ = ECMC_PV_REG_ERROR;
          }
        }
        catch(std::exception &e){
          errorCode_ = ECMC_PV_PUT_ERROR;
        }
        break;
      case ECMC_PV_CMD_REG:  // Not used
        try{
          connect();
        }
        catch(std::exception &e){
          errorCode_ = ECMC_PV_REG_ERROR;
        }
        break;    
      default:
        break;
    }    
  } 
}

void ecmcPv::monitorThread() {

  //  cout << "__exampleDouble recordName " << recordName << " provider " << provider << "__\n";
  //   PvaClientMonitorPtr monitor = pva->channel(recordName,provider,2.0)->monitor("");
  //   PvaClientMonitorDataPtr monitorData = monitor->getData();
    
  //   PvaClientPutPtr put = pva->channel(recordName,provider,2.0)->put("");
  //   PvaClientPutDataPtr putData = put->getData();
  //   for(size_t ntimes=0; ntimes<5; ++ntimes)
  //   {
  //        double value = ntimes;
  //        cout << "put " << value << endl;
  //        putData->putDouble(value); put->put();
  //        if(!monitor->waitEvent(.1)) {
  //              cout << "waitEvent returned false. Why???";
  //              continue;
  //        } else while(true) {
  //            cout << "monitor " << monitorData->getDouble() << endl;
  //            cout << "changed\n";
  //            monitorData->showChanged(cout);
  //            cout << "overrun\n";
  //            monitorData->showOverrun(cout);
  //            monitor->releaseEvent();
  //            if(!monitor->poll()) break;
  //        }
  //    }
  while(true) {
    if(destructs_) {
      break; 
    }
    if(!connected_) {
      continue;
    }
    if(!monitor_->waitEvent(.1)) {
      cout << "waitEvent returned false. Why???";
      continue;
    } else while(true) {
      cout << "monitor " << monitorData_->getDouble() << endl;
      cout << "changed\n";
      monitorData_->showChanged(cout);
      cout << "overrun\n";
      monitorData_->showOverrun(cout);
      monitor_->releaseEvent();
      if(!monitor_->poll()) break;
     }
  }
}

int ecmcPv::connect() {

  pva_ = PvaClient::get("pva ca");
  pvaChannel_ = pva_->createChannel(name_,providerName_);
  monitor_ = pva_->channel(name_,providerName_,2.0)->monitor("");
  monitorData_ = monitor_->getData();
  pvaChannel_->issueConnect();
  Status status = pvaChannel_->waitConnect(1.0);
  if(!status.isOK()) {
    cout << "Error: connect failed\n";
    errorCode_ = ECMC_PV_REG_ERROR;
    throw std::runtime_error("Error: Failed connect to:" + name_);
  }
  get_ = pvaChannel_->createGet();
  get_->issueConnect();
  status = get_->waitConnect();
  if(!status.isOK()) {
    cout << "Error: createGet failed\n";
    errorCode_ = ECMC_PV_REG_ERROR;
    throw std::runtime_error("Error: Failed create get to:" + name_);
  }
  getData_ = get_->getData();
  //printf("Get Data has value: %d, isValueScalar %d\n", getData_->hasValue(),getData_->isValueScalar());
  put_ = pvaChannel_->put();
  putData_ = put_->getData();

  //printf("Put Data has value: %d, isScalarArray %d\n", putData_->hasValue(),getData_->isValueScalarArray());
  //cout << "getData_->getvalue(): " << getData_->getValue()<< "\n";
  //cout << "getData_->getString(): " << getData_->getString()<< "\n";
  //cout << "getData_->getPVStructure(): " << getData_->getPVStructure()<< "\n";
  //cout << "getData_->getStructure(): " << getData_->getStructure()<< "\n";
  //cout << "getData_->getValue()->getField()->getType(): " << getData_->getValue()->getField()->getType() << "\n";
  //cout << "getData_->getValue()->getField(): " << getData_->getValue()->getField() << "\n"; 
  //cout << "getData_->getValue()->getField(): " << getData_->getValue()->getField() << "\n"; 

  // Ensure that type is scalar or enum
  type_ = getData_->getValue()->getField()->getType();
  if(!validateType()) {
    cout << "Error: Type not supported for PV: " + name_ +"\n";
    errorCode_ = ECMC_PV_TYPE_NOT_SUPPORTED;
    throw std::runtime_error("Error: Type not supported for PV: " + name_);
  }

  connected_ = true;
  return 0;
}

double ecmcPv::getDouble() {

  PVScalarPtr pvScalar = NULL;
  switch(type_) {
    case scalar:
      // Scalar types are normal AI/AO VAL fields
      valueLatestRead_ = pva_->channel(name_,providerName_)->getDouble();
      return valueLatestRead_;
      break;

    case structure:
      // Support enum records (return the index of the field)
      get_->get();
      pvScalar = getData_->getPVStructure()->getSubField<PVScalar>("value.index");      
      if(pvScalar) {
        valueLatestRead_ = pvScalar->getAs<double>();
        return valueLatestRead_;
      } else {
        errorCode_ = ECMC_PV_GET_ERROR;
        return 0;
      }
      break;

    case scalarArray:
      // not supported yet
      errorCode_ = ECMC_PV_GET_ERROR;
      return 0;      
      break;

    default:
      errorCode_ = ECMC_PV_GET_ERROR;
      return 0;
      break;

  }

  errorCode_ = ECMC_PV_GET_ERROR;
  return 0;
}


void ecmcPv::putDouble(double value) {

  PVScalarPtr pvScalar = NULL;
  switch(type_) {
    case scalar:
      putData_->putDouble(value);
      put_->put();      
      break;

    case structure:
      // Support enum BI/BO records
      //PVScalarPtr pvScalar(getData_->getPVStructure()->getSubField<PVScalar>("value.index"));
      pvScalar = putData_->getPVStructure()->getSubField<PVScalar>("value.index");
      if(pvScalar) {
        pvScalar->putFrom<double>(value);
        put_->put();
        valueLatestRead_ = value;
      } else {
        errorCode_ = ECMC_PV_GET_ERROR;        
      }
      break;

    case scalarArray:
      errorCode_ = ECMC_PV_GET_ERROR; 
      break;

    default:
      errorCode_ = ECMC_PV_GET_ERROR;
      break;
  }
  return;
}

int ecmcPv::validateType() {

  
  if(!getData_->hasValue()) {
    return 0;
  }
  PVScalarPtr pvScalar = NULL;  // Need to redo

  switch(type_) {
    case scalar:
      if(getData_->isValueScalar()) {
        return 1;
      }
      else {
        return 0;
      }      
      break;
    case structure:
      // Support enum BI/BO records enum type (index, choices)
      if(!(getData_->getValue()->getField()->getID()=="enum_t")) {
        return 0;
      }

      pvScalar = getData_->getPVStructure()->getSubField<PVScalar>("value.index");
      if (pvScalar) {
        return 1;
      } else {
        return 0;
      }       

      break;
    case scalarArray:
      return 0;
      break;

    default:
      return 0;
      break;

  }
  return 0;
}

// Static Avoid issues with std:to_string()
std::string ecmcPv::to_string(int value) {
  std::ostringstream os;
  os << value;
  return os.str();
}


//    if(getData_->isValueScalar()) {
//      use get/setDoube()
//    }

/*

Type type(field->getType());
    if(type==scalar) {
        PVScalarPtr pvScalar(std::tr1::static_pointer_cast<PVScalar>(pvField));
        getConvert()->fromString(pvScalar,value);
        bitSet->set(pvField->getFieldOffset());
        channelPut->put(pvStructure,bitSet);
        return;
    }
    if(type==scalarArray) {
        PVScalarArrayPtr pvScalarArray(std::tr1::static_pointer_cast<PVScalarArray>(pvField));
        std::vector<string> values;
        size_t pos = 0;
        size_t n = 1;
        while(true)
        {
            size_t offset = value.find(" ",pos);
            if(offset==string::npos) {
                values.push_back(value.substr(pos));
                break;
            }
            values.push_back(value.substr(pos,offset-pos));
            pos = offset+1;
            n++;    
        }
        pvScalarArray->setLength(n);
        getConvert()->fromStringArray(pvScalarArray,0,n,values,0);       
        bitSet->set(pvField->getFieldOffset());
        channelPut->put(pvStructure,bitSet);
        return;
    }
    if(type==structure) {
       PVScalarPtr pvScalar(pvStructure->getSubField<PVScalar>("value.index"));
       if(pvScalar) {
          getConvert()->fromString(pvScalar,value);
          bitSet->set(pvScalar->getFieldOffset());
          channelPut->put(pvStructure,bitSet);
          return;
       }
    }

void PvaClientData::setData(
    PVStructurePtr const & pvStructureFrom,
    BitSetPtr const & bitSetFrom)
{
   if(PvaClient::getDebug()) cout << "PvaClientData::setData\n";
   pvStructure = pvStructureFrom;
   bitSet = bitSetFrom;
   pvValue = pvStructure->getSubField("value");
}ECMC_PV_TIME_BETWEEN_RECONNECT

bool PvaClientData::hasValue()
{
    if(PvaClient::getDebug()) cout << "PvaClientData::hasValue\n";
    if(!pvValue) return false;
    return true;
}

bool PvaClientData::isValueScalar()
{
    if(PvaClient::getDebug()) cout << "PvaClientData::isValueScalar\n";
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalar) return true;
    return false;
}
*/
