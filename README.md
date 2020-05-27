e3-ecmcPlugin_Utils
======
ESS Site-specific EPICS module : ecmcPlugin_Utils

A shared library utility functions loadable into ecmc.

## PvAccess 
Implements functions for accessing pv:s over pvAccess from ecmc plc:s.

### Registering and writing pvs:
Registration and writes are implementad as async commands in order to minimize blocking time of ecmc realtime thread.

### Reading values:
A monitor is setup that is updating the current value of the pv continiously and making int accessible to read from a ecmc 
plc.

### PLC-functions:
  * handle = pv_reg_async( pvName, provider ) : Exe. async cmd to register PV. Returns handle to PV-object or error (if < 0). Provider needs to be set to either "pva" or "ca" (ca to be able to access pv:s in EPICS 3.* IOC:s).  
  * error  = pv_put_async( handle, value ) : Exe async pv put command.  Retruns error-code.
  * value  = pv_value( handle ): Get value from last monitor update.
  * busy   = pv_busy( handle ) : Return if PV-object is busy (busy if a put or a reg async commnd is executing).
  * error  = pv_err( handle ) : Returns error code of PV-object (error > 0).
  * pv_rst( handle ) : Reset error of PV-object. Normally not needed to execute.
  * connected = pv_connected(<handle>) : Return if pv is connected.

### Record support
The functions currently only support scalar values. Value field of following record types have been tested:
* AI
* AO
* BI
* BO 

### Example
The example in the "iocsh" dir shows how to use the pva functions. The example demonstartes how a ecmc plc (in an ecmc ioc) can connect to an external ioc. The ecmc plc registers, writes and reads values from the folowing records:
* IOC_DUMMY:AI
* IOC_DUMMY:AO
* IOC_DUMMY:BI
* IOC_DUMMY:BO

Start ecmc-ioc (from iocsh dir):
```
$ iocsh.bash ecmc_ioc.script
```
Start external-ioc (from iocsh dir):
```
$ iocsh.bash external_ioc.script
```

## EPICS utils:
  * started = ioc_get_started() : ecmc IOC up and running
  * state = ioc_get_state()   : ecmc IOC state (hook)

## Plugin Info
```
Plugin info: 
  Index                = 0
  Name                 = ecmcPlugin_Utils
  Description          = Utility plugin for use with ecmc. Funcs: pvAccess, ioc status.
  Option description   = No options
  Filename             = /epics/base-7.0.3.1/require/3.1.2/siteMods/ecmcPlugin_Utils/master/lib/linux-arm/libecmcPlugin_Utils.so
  Config string        = EMPTY
  Version              = 2
  Interface version    = 65536 (ecmc = 65536)
     max plc funcs     = 64
     max plc func args = 10
     max plc consts    = 64
  Construct func       = @0xb4e22b7c
  Enter realtime func  = @0xb4e22a60
  Exit realtime func   = @0xb4e22a68
  Realtime func        = @0xb4e22a58
  Destruct func        = @0xb4e22a70
  dlhandle             = @0x185b050
  Plc functions:
    funcs[00]:
      Name       = "pv_reg_asyn();"
      Desc       = handle = pv_reg_asyn(<pv name>, <provider name pva/ca>) : register new pv.
      func       = @0x186f3b8
    funcs[01]:
      Name       = "pv_put_asyn(arg0, arg1);"
      Desc       = error = pv_put_asyn(<handle>, <value>) : Execute async pv_put cmd.
      Arg count  = 2
      func       = @0xb4e22a94
    funcs[02]:
      Name       = "pv_value(arg0);"
      Desc       = value = pv_value(<handle>) : Get result from last pv_get_async() or pv_put_async() cmd.
      Arg count  = 1
      func       = @0xb4e22ab4
    funcs[03]:
      Name       = "pv_busy(arg0);"
      Desc       = busy = pv_busy(<handle>) : Get status of last async command (pv_reg_asyn(), pv_put_asyn()).
      Arg count  = 1
      func       = @0xb4e22ac0
    funcs[04]:
      Name       = "pv_err(arg0);"
      Desc       = error = pv_err(<handle>) : Get error code.
      Arg count  = 1
      func       = @0xb4e22af8
    funcs[05]:
      Name       = "pv_rst(arg0);"
      Desc       = pv_rst(<handle>) : Reset error code.
      Arg count  = 1
      func       = @0xb4e22b14
    funcs[06]:
      Name       = "pv_connected(arg0);"
      Desc       = connected = pv_connected(<handle>) : Get pv connected.
      Arg count  = 1
      func       = @0xb4e22adc
    funcs[07]:
      Name       = "ioc_get_state();"
      Desc       = state = ioc_get_state() : Get ecmc epics ioc state.
      Arg count  = 0
      func       = @0xb4e22b68
    funcs[08]:
      Name       = "ioc_get_started();"
      Desc       = started = ioc_get_started() : Get ecmc epics ioc started.
      Arg count  = 0
      func       = @0xb4e22b38
  Plc constants:
```
