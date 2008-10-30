/* define your pre-dx.h include file for inclusion here*/ 
#ifdef PRE_DX_H
#include "HapticDevice_predx.h"
#endif
#include "dx/dx.h"

#if defined(WIN32)
# include <windows.h>
#else
# include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>

#include <HD/hd.h>

#include <HDU/hduVector.h>
#include <HDU/hduError.h>

/* define your post-dx.h include file for inclusion here*/ 
#ifdef POST_DX_H
#include "HapticDevice_postdx.h"
#endif

extern Pointer DXGetModuleId();
extern Error DXReadyToRun(Pointer id);
extern Error DXFreeModuleId(Pointer id);


static Pointer id = NULL;


typedef struct 
{
    HDboolean button_pressed;
    hduVector3Dd position;
    HDErrorInfo error;
} DeviceData;

static DeviceData HapticDeviceData;

/* Instantiate the structure used to capture data from the device. */
DeviceData current_data;
DeviceData prev_data;

HHD hHD;
HDSchedulerHandle hUpdateHandle = 0;
HDErrorInfo error;


void sigcatch()
{
    DXReadyToRun(id);
}	

void cleanup()
{
  hdStopScheduler();
  hdUnschedule(hUpdateHandle);
  hdDisableDevice(hHD);
}

unsigned malarm(unsigned msecs, unsigned reload)
{
  struct itimerval new, old;
  new.it_interval.tv_usec = 1000 * (reload % 1000);
  new.it_interval.tv_sec = reload / 1000;
  new.it_value.tv_usec = 1000 * (msecs % 1000);
  new.it_value.tv_sec = msecs / 1000;

  if (setitimer(ITIMER_REAL, &new, &old) == 0)
    return (old.it_value.tv_sec * 1000 + old.it_value.tv_usec/1000);
  /* else */
  return (-1);
}

HDCallbackCode HDCALLBACK updateDeviceCallback(void *pUserData)
{   
  int num_buttons = 0;
  
  hdBeginFrame(hdGetCurrentDevice());
  
  hdGetIntegerv(HD_CURRENT_BUTTONS, &num_buttons);
  
  HapticDeviceData.button_pressed = 
    (num_buttons & HD_DEVICE_BUTTON_1) ? HD_TRUE : HD_FALSE;
    
  hdGetDoublev(HD_CURRENT_POSITION, HapticDeviceData.position);
    
  HapticDeviceData.error = hdGetError();
    
  hdEndFrame(hdGetCurrentDevice());
  
  return HD_CALLBACK_CONTINUE;    
}


HDCallbackCode HDCALLBACK copyDeviceDataCallback(void *pUserData)
{
  DeviceData *pDevData = (DeviceData *) pUserData;
  
  memcpy(pDevData, &HapticDeviceData, sizeof(DeviceData));
  
  return HD_CALLBACK_DONE;
}


static Error traverse(Object *, Object *);
static Error doLeaf(Object *, Object *);

/*
 * Declare the interface routine.
 */
int
HapticDevice_worker(
    int, int, float *,
    int, int, int *,
    int, float *,
    int, float *);

#if defined (__cplusplus) || defined (c_plusplus)
extern "C"
#endif
Error
m_HapticDevice(Object *in, Object *out)
{
  int i;

  Array data=NULL, positions=NULL;

  /*
   * Initialize all outputs to NULL
   */
  out[0] = NULL;

  /*
   * Error checks: required inputs are verified.
   */

  /* Parameter "obj" is required. */
  if (in[0] == NULL)
  {
    DXSetError(ERROR_MISSING_DATA, "\"obj\" must be specified");
    return ERROR;
  }


  /*
   * Since output "pointer" is structure Field/Group, it initially
   * is a copy of input "obj".
   */
  out[0] = DXCopy(in[0], COPY_STRUCTURE);
  if (! out[0])
    goto error;

  /*
   * If in[0] was an array, then no copy is actually made - Copy 
   * returns a pointer to the input object.  Since this can't be written to
   * we postpone explicitly copying it until the leaf level, when we'll need
   * to be creating writable arrays anyway.
   */
  if (out[0] == in[0])
    out[0] = NULL;

  /* arrange a handler to catch the signal */
  signal(SIGALRM, sigcatch);
  
  /* the first time, get the module id for the DXReadyToRun call */

  if (!id) {
    /* Initialize the device, must be done before attempting to call any hd 
       functions. */
    hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    if (HD_DEVICE_ERROR(error = hdGetError()))
      {
	//hduPrintError(stderr, &error, "Failed to initialize the device");
	DXMessage("Failed to initialize the device");
	return ERROR;           
      }
    
    /* Schedule the main scheduler callback that updates the device state. */
    hUpdateHandle = hdScheduleAsynchronous(updateDeviceCallback, 0, HD_MAX_SCHEDULER_PRIORITY);
  
    /* Start the servo loop scheduler. */
    hdStartScheduler();
    if (HD_DEVICE_ERROR(error = hdGetError()))
      {
	//hduPrintError(stderr, &error, "Failed to start the scheduler");
	DXMessage("Failed to start the scheduler");
	return ERROR;           
      }

    atexit(cleanup);

    /* Perform a synchronous call to copy the most current device state. */
    hdScheduleSynchronous(copyDeviceDataCallback,
			  &current_data, HD_MIN_SCHEDULER_PRIORITY);
    
    memcpy(&prev_data, &current_data, sizeof(DeviceData));
  }
  
  if (!id) {
    id = DXGetModuleId();
    if (!id) {
      out[0] = NULL;
      return ERROR;
    }
  }

  /*
   * Call the hierarchical object traversal routine
   */
  if (!traverse(in, out))
    goto error;


  /* Perform a synchronous call to copy the most current device state.
     This synchronous scheduler call ensures that the device state
     is obtained in a thread-safe manner. */
  hdScheduleSynchronous(copyDeviceDataCallback,
			&current_data,
			HD_MIN_SCHEDULER_PRIORITY);
  
  /* make a new data array (scalar) */
  data = DXNewArray(TYPE_FLOAT, CATEGORY_REAL, 0);
  if (!data)
    goto error;

  if (!DXAddArrayData(data, 0, 1, current_data.button_pressed?0:1))
     goto error;

  /* set the dependency of the data to be on positions */
  if (!DXSetStringAttribute((Object)data, "dep", "positions"))
    goto error;

  /* 
   * Make the new positions array for the output. The positions are
   * three-dimensional. 
   */
  positions = DXNewArray(TYPE_FLOAT, CATEGORY_REAL, 1, 3);
  if (! positions)
    goto error;

  if (! DXAddArrayData(positions, 0, 1, DXPt(current_data.position[0], current_data.position[1], current_data.position[2])))
    goto error;
  
  out[0] = DXNewField();
  DXSetComponentValue((Field)out[0], "positions", (Object)positions);
  positions = NULL;
  DXSetComponentValue((Field)out[0], "data", (Object)data);
  data = NULL;

  /* Finalize the field */
  DXEndField((Field)out[0]);
  

  if (HD_DEVICE_ERROR(current_data.error))
    {
      hduPrintError(stderr, &current_data.error, "Device error detected");
      
      if (hduIsSchedulerError(&current_data.error))
	{
	  break;
	}
    }

  memcpy(&prev_data, &current_data, sizeof(DeviceData));    

  malarm(10, 0);

  return OK;

error:
  /*
   * On error, any successfully-created outputs are deleted.
   */
  for (i = 0; i < 1; i++)
  {
    if (in[i] != out[i])
      DXDelete(out[i]);
    out[i] = NULL;
  }
  return ERROR;
}


static Error
traverse(Object *in, Object *out)
{
  switch(DXGetObjectClass(in[0]))
  {
    case CLASS_FIELD:
    case CLASS_ARRAY:
    case CLASS_STRING:
      /*
       * If we have made it to the leaf level, call the leaf handler.
       */
      if (! doLeaf(in, out))
  	     return ERROR;

      return OK;

    case CLASS_GROUP:
    {
      int   i, j;
      int   memknt;
      Class groupClass  = DXGetGroupClass((Group)in[0]);

      DXGetMemberCount((Group)in[0], &memknt);


       /*
        * Create new in and out lists for each child
        * of the first input. 
        */
        for (i = 0; i < memknt; i++)
        {
          Object new_in[1], new_out[1];

         /*
          * For all inputs that are Values, pass them to 
          * child object list.  For all that are Field/Group, get 
          * the appropriate decendent and place it into the
          * child input object list.
          */

          /* input "obj" is Field/Group */
          if (in[0])
            new_in[0] = DXGetEnumeratedMember((Group)in[0], i, NULL);
          else
            new_in[0] = NULL;

         /*
          * For all outputs that are Values, pass them to 
          * child object list.  For all that are Field/Group,  get
          * the appropriate decendent and place it into the
          * child output object list.  Note that none should
          * be NULL (unlike inputs, which can default).
          */

          /* output "pointer" is Field/Group */
          new_out[0] = DXGetEnumeratedMember((Group)out[0], i, NULL);

          if (! traverse(new_in, new_out))
            return ERROR;

         /*
          * Now for each output that is not a Value, replace
          * the updated child into the object in the parent.
          */

          /* output "pointer" is Field/Group */
          DXSetEnumeratedMember((Group)out[0], i, new_out[0]);

        }
      return OK;
    }

    case CLASS_XFORM:
    {
      int    i, j;
      Object new_in[1], new_out[1];


      /*
       * Create new in and out lists for the decendent of the
       * first input.  For inputs and outputs that are Values
       * copy them into the new in and out lists.  Otherwise
       * get the corresponding decendents.
       */

      /* input "obj" is Field/Group */
      if (in[0])
        DXGetXformInfo((Xform)in[0], &new_in[0], NULL);
      else
        new_in[0] = NULL;

      /*
       * For all outputs that are Values, copy them to 
       * child object list.  For all that are Field/Group,  get
       * the appropriate decendent and place it into the
       * child output object list.  Note that none should
       * be NULL (unlike inputs, which can default).
       */

      /* output "pointer" is Field/Group */
      DXGetXformInfo((Xform)out[0], &new_out[0], NULL);

      if (! traverse(new_in, new_out))
        return ERROR;

      /*
       * Now for each output that is not a Value replace
       * the updated child into the object in the parent.
       */

      /* output "pointer" is Field/Group */
      DXSetXformObject((Xform)out[0], new_out[0]);

      return OK;
    }

    case CLASS_SCREEN:
    {
      int    i, j;
      Object new_in[1], new_out[1];


      /*
       * Create new in and out lists for the decendent of the
       * first input.  For inputs and outputs that are Values
       * copy them into the new in and out lists.  Otherwise
       * get the corresponding decendents.
       */

      /* input "obj" is Field/Group */
      if (in[0])
        DXGetScreenInfo((Screen)in[0], &new_in[0], NULL, NULL);
      else
        new_in[0] = NULL;


      /*
       * For all outputs that are Values, copy them to 
       * child object list.  For all that are Field/Group,  get
       * the appropriate decendent and place it into the
       * child output object list.  Note that none should
       * be NULL (unlike inputs, which can default).
       */

       /* output "pointer" is Field/Group */
       DXGetScreenInfo((Screen)out[0], &new_out[0], NULL, NULL);

       if (! traverse(new_in, new_out))
         return ERROR;

      /*
       * Now for each output that is not a Value, replace
       * the updated child into the object in the parent.
       */

      /* output "pointer" is Field/Group */
       DXSetScreenObject((Screen)out[0], new_out[0]);

       return OK;
     }

     case CLASS_CLIPPED:
     {
       int    i, j;
       Object new_in[1], new_out[1];


       /* input "obj" is Field/Group */
       if (in[0])
         DXGetClippedInfo((Clipped)in[0], &new_in[0], NULL);
       else
         new_in[0] = NULL;


      /*
       * For all outputs that are Values, copy them to 
       * child object list.  For all that are Field/Group,  get
       * the appropriate decendent and place it into the
       * child output object list.  Note that none should
       * be NULL (unlike inputs, which can default).
       */

       /* output "pointer" is Field/Group */
       DXGetClippedInfo((Clipped)out[0], &new_out[0], NULL);

       if (! traverse(new_in, new_out))
         return ERROR;

      /*
       * Now for each output that is not a Value, replace
       * the updated child into the object in the parent.
       */

       /* output "pointer" is Field/Group */
       DXSetClippedObjects((Clipped)out[0], new_out[0], NULL);

       return OK;
     }

     default:
     {
       DXSetError(ERROR_BAD_CLASS, "encountered in object traversal");
       return ERROR;
     }
  }
}

static int
doLeaf(Object *in, Object *out)
{
  int i, result=0;
  Array array;
  Field field;
  Pointer *in_data[1], *out_data[1];
  int in_knt[1], out_knt[1];
  Type type;
  Category category;
  int rank, shape;
  Object attr, src_dependency_attr = NULL;
  char *src_dependency = NULL;
  /*
   * Irregular positions info
   */
  int p_knt, p_dim;
  float *p_positions;
  /*
   * Irregular connections info
   */
  int c_knt, c_dim, c_nv;
  float *c_connections;

  /*
   * positions and/or connections are required, so the first must
   * be a field.
   */
  if (DXGetObjectClass(in[0]) != CLASS_FIELD)
  {
      DXSetError(ERROR_DATA_INVALID,
           "positions and/or connections unavailable in array object");
      goto error;
  }
  else
  {

    field = (Field)in[0];

    if (DXEmptyField(field))
      return OK;

    /* 
     * Determine the dependency of the source object's data
     * component.
     */
    src_dependency_attr = DXGetComponentAttribute(field, "data", "dep");
    if (! src_dependency_attr)
    {
      DXSetError(ERROR_MISSING_DATA, "\"obj\" data component is missing a dependency attribute");
      goto error;
    }

    if (DXGetObjectClass(src_dependency_attr) != CLASS_STRING)
    {
      DXSetError(ERROR_BAD_CLASS, "\"obj\" dependency attribute");
      goto error;
    }

    src_dependency = DXGetString((String)src_dependency_attr);

    array = (Array)DXGetComponentValue(field, "positions");
    if (! array)
    {
      DXSetError(ERROR_BAD_CLASS, "\"obj\" contains no positions component");
      goto error;
    }

    /* 
     * The user requested irregular positions.  So we
     * get the count, the dimensionality and a pointer to the
     * explicitly enumerated positions.  If the positions
     * are in fact regular, this will expand them.
     */
    DXGetArrayInfo(array, &p_knt, NULL, NULL, NULL, &p_dim);

    p_positions = (float *)DXGetArrayData(array);
    if (! p_positions)
      goto error;

    array = (Array)DXGetComponentValue(field, "connections");
    if (! array)
    {
      DXSetError(ERROR_BAD_CLASS, "\"obj\" contains no connections component");
      goto error;
    }

    /* 
     * The user requested irregular connections.  So we
     * get the count, the dimensionality and a pointer to the
     * explicitly enumerated elements.  If the positions
     * are in fact regular, this will expand them.
     */
    DXGetArrayInfo(array, &c_knt, NULL, NULL, NULL, &c_nv);

    c_connections = (float *)DXGetArrayData(array);
    if (! c_connections)
      goto error;

  }
  /*
   * If the input argument is not NULL then we get the 
   * data array: either the object itself, if its an 
   * array, or the data component if the argument is a field
   */
  if (! in[0])
  {
    array = NULL;
    in_data[0] = NULL;
    in_knt[0] = NULL;
  }
  else
  {
    if (DXGetObjectClass(in[0]) == CLASS_ARRAY)
    {
      array = (Array)in[0];
    }
    else if (DXGetObjectClass(in[0]) == CLASS_STRING)
    {
      in_data[0] = (Pointer *)DXGetString((String)in[0]);
      in_knt[0] = 1;
    }
    else
    {
      if (DXGetObjectClass(in[0]) != CLASS_FIELD)
      {
        DXSetError(ERROR_BAD_CLASS, "\"obj\" should be a field");
        goto error;
      }

      array = (Array)DXGetComponentValue((Field)in[0], "data");
      if (! array)
      {
        DXSetError(ERROR_MISSING_DATA, "\"obj\" has no data component");
        goto error;
      }

      if (DXGetObjectClass((Object)array) != CLASS_ARRAY)
      {
        DXSetError(ERROR_BAD_CLASS, "data component of \"obj\" should be an array");
        goto error;
      }
    }

    /* 
     * get the dependency of the data component
     */
    attr = DXGetAttribute((Object)array, "dep");
    if (! attr)
    {
      DXSetError(ERROR_MISSING_DATA, "data component of \"obj\" has no dependency");
      goto error;
    }

    if (DXGetObjectClass(attr) != CLASS_STRING)
    {
      DXSetError(ERROR_BAD_CLASS, "dependency attribute of data component of \"obj\"");
      goto error;
    }


    if (DXGetObjectClass(in[0]) != CLASS_STRING)    {
       DXGetArrayInfo(array, &in_knt[0], &type, &category, &rank, &shape);
       if (type != TYPE_FLOAT || category != CATEGORY_REAL ||
             !((rank == 0) || ((rank == 1)&&(shape == 1))))
       {
         DXSetError(ERROR_DATA_INVALID, "input \"obj\"");
         goto error;
       }

       in_data[0] = DXGetArrayData(array);
       if (! in_data[0])
          goto error;

    }
  }
  /*
   * Create an output data array typed according to the
   * specification given
   */
  array = DXNewArray(TYPE_FLOAT, CATEGORY_REAL, 0, 0);
  if (! array)
    goto error;

  /*
   * This output data array will be dep positions - and sized
   * appropriately - if the appropriate size is known
   */
  if (p_knt == -1)
  {
    DXSetError(ERROR_DATA_INVALID,
      "cannot make output \"pointer\" dep on positions because no positions were found in input[0]");
    goto error;
  }

  out_knt[0] = p_knt;

  if (! DXSetAttribute((Object)array, "dep", (Object)DXNewString("positions")))
    goto error;
  /*
   * Actually allocate the array data space
   */
  if (! DXAddArrayData(array, 0, out_knt[0], NULL))
    goto error;

  /*
   * If the output vector slot is not NULL, then it better be a field, and
   * we'll add the new array to it as its data component (delete any prior
   * data component so that its attributes won't overwrite the new component's)
   * Otherwise, place the new array into the out vector.
   */
  if (out[0])
  {
    if (DXGetObjectClass(out[0]) != CLASS_FIELD)
    {
      DXSetError(ERROR_INTERNAL, "non-field object found in output vector");
      goto error;
    }

    if (DXGetComponentValue((Field)out[0], "data"))
      DXDeleteComponent((Field)out[0], "data");

    if (! DXSetComponentValue((Field)out[0], "data", (Object)array))
      goto error;

  }
  else
    out[0] = (Object)array;
  /*
   * Now get the pointer to the contents of the array
   */
  out_data[0] = DXGetArrayData(array);
  if (! out_data[0])
    goto error;

  /*
   * Call the user's routine.  Check the return code.
   */
  result = HapticDevice_worker(
    p_knt, p_dim, (float *)p_positions,
    c_knt, c_nv, (int *)c_connections,
    in_knt[0], (float *)in_data[0],
    out_knt[0], (float *)out_data[0]);

  if (! result)
     if (DXGetError()==ERROR_NONE)
        DXSetError(ERROR_INTERNAL, "error return from user routine");

  /*
   * In either event, clean up allocated memory
   */

error:
  return result;
}

int
HapticDevice_worker(
    int p_knt, int p_dim, float *p_positions,
    int c_knt, int c_nv, int *c_connections,
    int obj_knt, float *obj_data,
    int pointer_knt, float *pointer_data)
{
  /*
   * The arguments to this routine are:
   *
   *  p_knt:           total count of input positions
   *  p_dim:           dimensionality of input positions
   *  p_positions:     pointer to positions list
   *  c_knt:           total count of input connections elements
   *  c_nv:            number of vertices per element
   *  c_connections:   pointer to connections list
   *
   * The following are inputs and therefore are read-only.  The default
   * values are given and should be used if the knt is 0.
   *
   * obj_knt, obj_data:  count and pointer for input "obj"
   *                   no default value given.
   *
   *  The output data buffer(s) are writable.
   *  The output buffer(s) are preallocated based on
   *     the dependency (positions or connections),
   *     the size of the corresponding positions or
   *     connections component in the first input, and
   *     the data type.
   *
   * pointer_knt, pointer_data:  count and pointer for output "pointer"
   */

  /*
   * User's code goes here
   */
     
     
  /*
   * successful completion
   */
   return 1;
     
  /*
   * unsuccessful completion
   */
error:
   return 0;
  
}
