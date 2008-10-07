// dxstereo.c: OpenDX stereo visualization module

//	Copyright (C) 2008 David Suarez Pascal <david.suarez@yahoo.com>
//
//	This file is part of DXStereo
//
//	DXStereo is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	DXStereo is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with DXStereo; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
//	02111-1307 USA


/* Nota: se han agregado a este archivo marcas (<DXSTEREO>) comentadas para indicar
*        que partes del codigo corresponde a la plantilla generado por el Module Builder
*        de OpenDX y que partes implementan la funcionalidad especifica del modulo
*/

/* define your pre-dx.h include file for inclusion here*/
#ifdef PRE_DX_H
#include "dxstereo_predx.h"
#endif
#include "dx/dx.h"
/* define your post-dx.h include file for inclusion here*/
///<DXSTEREO> Encabezados
#ifdef FREEGLUT
#include <GL/freeglut_ext.h>
#else
#include <GL/glut.h>
#endif
#include <math.h>
#include <string.h>
#include <unistd.h>
///</DXSTEREO> Fin de los encabezados
#ifdef POST_DX_H
#include "dxstereo_postdx.h"
#endif

///<DXSTEREO> Definiciones de tipos, variables y constantes
#define DTR 0.0174532925

struct camera //Estructura que almacena los datos para las camaras de cada ojo
{
    GLdouble leftfrustum;
    GLdouble rightfrustum;
    GLdouble bottomfrustum;
    GLdouble topfrustum;
    GLfloat modeltranslation;
} leftCam, rightCam;

float depthZ = -10.0; //Profundidad del objeto

float luz[] = {0.3, 0.3, 0.3, 1.0}; //Define una luz de ambiente tenue
double fovy = 45; //Campo de vision en el eje y
double aspect = 1.0; //Razon ancho/alto
double nearZ = 1.0; //Plano de corte cercano
double farZ = 30.0; //Plano de corte lejano
double screenZ = 10.0; //Plano de proyeccion
double IOD; //Distancia interocular
GLsizei width, height; //Ancho y alto de la ventana

int dlist_knt = 0; //Contador de listas de despliegue

//Datos de la caja que encierra a todos los objetos
float minX=0, minY=0,minZ=0; //Limites inferiores
float maxX=0, maxY=0,maxZ=0; //Limites superiores
float center[3], point[3], radius; //Coordenadas del centro
float scale = 1.0; //Escala del objeto graficado
float dist(float *, float *); //Calcula la distancia entre dos puntos

//Funciones que manejan los cambios en la posicion de los objetos
void resetView( GLvoid );
void polarView( GLfloat, GLfloat, GLfloat, GLfloat );

//Variables globales que controlan los cambios de posicion de los objetos
static GLint action;
static GLfloat near, far, distance, twistAngle, incAngle, azimAngle;
static GLdouble xStart = 0.0, yStart = 0.0;
enum actions {MOVE_EYE, TWIST_EYE, ZOOM, MOVE_NONE};

//Funciones utilizadas por las callbacks de GLUT
GLvoid display_mono(GLvoid); //Despliega solamente una vista del objeto
GLvoid display_stereo(GLvoid); //Despliega las dos vistas usando quad-buffering
GLvoid keyboard(GLubyte, GLint, GLint); //Maneja eventos del teclado
GLvoid reshape(GLsizei, GLsizei); //Maneja cambios de forma de la ventana de despliegue
GLvoid mouse(GLint, GLint, GLint, GLint); //Maneja los eventos del raton
GLvoid motion(GLint, GLint); //Maneja los desplazamientos del raton

//Funciones que preparan el despliegue de los objetos
void drawScene(void); //Ejecuta las listas de despliegue para el objeto
void setFrustum(void); //Ajusta el volumen de vision
#ifdef DXSTEREO_DEBUG
void checkError(char *); //Checa si ha habido errores de OpenGL
#endif

#ifdef DXSTEREO_DEBUG
//Variables y funciones usadas en la depuracion
char strerr[255]; //Buffer de caracteres (para depuracion)
void writeLog(char *); //Escribe datos a un archivo (para depuracion)
#endif
///</DXSTEREO>

//Funciones credas por el OpenDX Module Builder para procesar el arbol de objetos
static Error traverse(Object *);
static Error doLeaf(Object *);

/*
 * Declare the interface routine.
 */
int dxstereo_worker(
		int, int, float *,
		int, int, int *,
		int, int, float *,
		int, int, float *,
		int, int, float *,
		int, float *);

#if defined (__cplusplus) || defined (c_plusplus)
extern "C"
#endif

Error m_DXStereo(Object *in, Object *out)
{
  Array array;
  int array_size;
  Type type;
  Category category;
  int rank, shape;
 ///<DXSTEREO> Variables de inicializacion GLUT
  int fakecp=1;
  char * fakev[]={"DXStereo"};
  char * mode; //Cadena para almacenar el valor del modo de despliegue seleccionado
  int stereo = 0; //Bandera de seleccion del modo estereo
  GLboolean stereo_buffering; //Bandera de soporte en HW del modo estereo
 ///</DXSTEREO>

  /*
   * Error checks: required inputs are verified.
   */

  /* Parameter "obj" is required. */
  if (in[0] == NULL)
    {
      DXSetError(ERROR_MISSING_DATA, "\"obj\" must be specified");
      return ERROR;
    }

///<DXSTEREO> Procesamiento del parametro de modo
  if (in[1] != NULL)
    {
      if (DXGetObjectClass(in[1]) != CLASS_STRING)
	{
	  DXSetError(ERROR_BAD_TYPE, "'mode' parameter must be of string type");
	  return ERROR;
	}
      else
	{
	  mode = DXGetString((String) in[1]);
	  if (strcmp(mode, "stereo") == 0)
	    {
	      glGetBooleanv(GL_STEREO, &stereo_buffering);//Checa si el modo estereo esta soportado en HW
	      if (stereo_buffering)
		stereo = 1; //Activa el modo estereo
	      else
		{
		  DXSetError(ERROR_ASSERTION, "stereo mode selected, but not supported by hardware");
		  return ERROR;
		}
	    }
	  else
	    stereo = 0; //Desactiva el modo estereo
	}
    }
  else
    stereo = 0; //Valor por defecto
///</DXSTEREO>

///<DXSTEREO> Procesamiento del parametro IOD
 if (in[2] != NULL)
    {
      if (DXGetObjectClass(in[2]) != CLASS_ARRAY)
	{
	  DXSetError(ERROR_BAD_TYPE, "'IOD' parameter must be of scalar type");
	  return ERROR;
	}
      else
	{
	    array = (Array)in[2];
	    DXGetArrayInfo(array, &array_size, &type, &category, &rank, &shape);
           if (type != TYPE_FLOAT || category != CATEGORY_REAL ||
             !((rank == 0) || ((rank == 1)&&(shape == 1))))
           {
             DXSetError(ERROR_DATA_INVALID, "input \"IOD\"");
             goto error;
           }
	   if (array_size == 1)
	    IOD = *((float *) (DXGetArrayData(array)));
	}
    }
  else
    IOD = 0.5; //Valor por defecto
///</DXSTEREO>

 ///<DXSTEREO> Inicializacion de GLUT
  glutInit(&fakecp, fakev); //Inicializa GLUT

 //Configura los buffers de despliegue de GLUT
  if (!stereo)
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
  else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_STEREO);

  glutCreateWindow("DXStereo"); //Crea la ventana
  glClearColor(0.0,0.0,0.0,1.0); //Limpia la ventana
  glEnable(GL_DEPTH_TEST); //Activa el buffer de profundidad
  glEnable(GL_BLEND); //Activa la mezcla
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //glEnable(GL_LINE_SMOOTH);
  //glEnable(GL_POINT_SMOOTH);
  //glEnable(GL_POLYGON_SMOOTH);
  glEnable(GL_LIGHTING); //Activa la iluminacion
  glEnable(GL_LIGHT0); //Enciende un luz
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luz); //Selecciona la luz predefinida
 ///</DXSTEREO>

  /*
   * Call the hierarchical object traversal routine
   */
  if (!traverse(in))
    {
      DXMessage("Error on traversing!");
      goto error;
    }
#ifdef DXSTEREO_DEBUG
  DXMessage("Traversed!");
#endif

 ///<DXSTEREO>Calcula los datos de posicion y taman~o del objeto
 //Centro
  center[0] = (maxX+minX)/2.0;
  center[1] = (maxY+minY)/2.0;
  center[2] = (maxZ+minZ)/2.0;
 //Esquina
  point[0] = minX;
  point[1] = minY;
  point[2] = minZ;
 //Radio
  radius = dist(point, center);
  //Modifica la profundidad del objeto y el plano lejano de corte de acuerdo a las dimensiones del objeto
  depthZ = -1.0 * (nearZ + 2.5 * radius);
  farZ = nearZ + 16 * radius;

#ifdef DXSTEREO_DEBUG
  sprintf(strerr, "Bounding box: (%.3f,%.3f,%.3f,%.3f,%.3f,%.3f)\n", minX, maxX, minY, maxY, minZ, maxZ);
  writeLog(strerr);
#endif
 ///</DXSTEREO>

 ///<DXSTEREO> Establece las funciones de manejo de eventos de GLUT
  if (!stereo) //Establece si se desplegaran una o dos vistas (mono o estereo)
    {
      glutDisplayFunc(display_mono);
      IOD = 0;
#ifdef DXSTEREO_DEBUG
      DXMessage("Working on mono!");
#endif
    }
  else
    {
      glutDisplayFunc(display_stereo);
#ifdef DXSTEREO_DEBUG
      DXMessage("Working on stereo!");
#endif
    }

  //Establece las funciones de callback de GLUT
  glutKeyboardFunc(keyboard); //Eventos del teclado
  glutMotionFunc(motion); //Eventos de desplazamiento
  glutMouseFunc(mouse); //Eventos del raton
  glutReshapeFunc(reshape); //Eventos de cambio de forma de la ventana
  //Inicia los parametros de desplazamiento y rotacion del objeto
  resetView();

/*Utiliza instrucciones del compilador para definir si el modulo se debe separar del proceso del ejecutivo o no
 *Aqui es importante evaluar si se debe convertir el modulo en externo(outboard), o en restablecer de alguna forma
 *la comunicacion con el ejecutivo.
 */
#ifdef DXSTEREO_FORK
   if (0 == fork())
    {
      glutMainLoop(); //Inicia el ciclo principal de GLUT
    }
   else
    {
      return OK;
    }
#endif
#ifndef DXSTEREO_FORK
    glutMainLoop(); //Inicia el ciclo principal de GLUT
    return OK;
#endif
///</DXSTEREO>

//Ha sido tentador estructurar esta parte del codigo, pero como es parte del codigo generado por el Module Builder, no se ha hecho
 error:
  return ERROR;
}

//Esta funcion se encarga de hacer la recursion sobre el arbol de objetos, no se ha necesitado modificarla
static Error traverse(Object *in)
{
  switch(DXGetObjectClass(in[0]))
    {
    case CLASS_FIELD:
    case CLASS_ARRAY:
    case CLASS_STRING:
      /*
       * If we have made it to the leaf level, call the leaf handler.
       */
      if (! doLeaf(in))
	return ERROR;

      return OK;

    case CLASS_GROUP:
      {
	int   i, j;
	int   memknt;
	Class groupClass  = DXGetGroupClass((Group)in[0]);

	DXGetMemberCount((Group)in[0], &memknt);


	/*
	 * Create new in lists for each child
	 * of the first input.
	 */
        for (i = 0; i < memknt; i++)
	  {
	    Object new_in[1];

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

	    if (! traverse(new_in))
	      return ERROR;

	  }
	return OK;
      }

    case CLASS_XFORM:
      {
	int    i, j;
	Object new_in[1];


	/*
	 * Create new in lists for the decendent of the
	 * first input.  For inputs and outputs that are Values
	 * copy them into the new in and out lists.  Otherwise
	 * get the corresponding decendents.
	 */

	/* input "obj" is Field/Group */
	if (in[0])
	  DXGetXformInfo((Xform)in[0], &new_in[0], NULL);
	else
	  new_in[0] = NULL;

	if (! traverse(new_in))
	  return ERROR;

	return OK;
      }

    case CLASS_SCREEN:
      {
	int    i, j;
	Object new_in[1];


	/*
	 * Create new in lists for the decendent of the
	 * first input.  For inputs that are Values
	 * copy them into the new in lists.  Otherwise
	 * get the corresponding decendents.
	 */

	/* input "obj" is Field/Group */
	if (in[0])
	  DXGetScreenInfo((Screen)in[0], &new_in[0], NULL, NULL);
	else
	  new_in[0] = NULL;


	if (! traverse(new_in))
	  return ERROR;

	return OK;
      }

    case CLASS_CLIPPED:
      {
	int    i, j;
	Object new_in[1];


	/* input "obj" is Field/Group */
	if (in[0])
	  DXGetClippedInfo((Clipped)in[0], &new_in[0], NULL);
	else
	  new_in[0] = NULL;


	if (! traverse(new_in))
	  return ERROR;

	return OK;
      }

    default:
      {
	DXSetError(ERROR_BAD_CLASS, "encountered in object traversal");
	return ERROR;
      }
    }
}

//Esta funcion se encarga de procesar cada rama terminal del arbol de objetos
static int doLeaf(Object *in)
{
  int i, result=0;
  Array array;
  Field field;
  Pointer *in_data[1];
  int in_knt[1];
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

///<DXSTEREO> Variables usadas para el procesamiento de los componentes de normales, colores y opacidades
  /*
   * Colors info
   */
  int col_knt, col_dim;
  float *c_colors;
  /*
   * Opacities info
   */
  int opa_knt=0, opa_dim;
  float *o_opacities;
  /*
   * Normals info
   */
  int norm_knt, norm_dim;
  float *n_normals;
///</DXSTEREO>

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

///<DXSTEREO> Procesamiento del componente de colores
      array = (Array)DXGetComponentValue(field, "colors");
      if (! array)
	{
	  DXSetError(ERROR_BAD_CLASS, "\"obj\" contains no colors component");
	  goto error;
	}

      DXGetArrayInfo(array, &col_knt, NULL, NULL, NULL, &col_dim);

      c_colors = (float *)DXGetArrayData(array);
      if (! c_colors)
	goto error;
///</DXSTEREO>

///<DXSTEREO> Procesamiento del componente de opacidades
      array = (Array)DXGetComponentValue(field, "opacities");
      if (array)
	{
	  DXGetArrayInfo(array, &opa_knt, NULL, NULL, NULL, &opa_dim);
	  o_opacities = (float *)DXGetArrayData(array);
	  if (! o_opacities)
	    goto error;
	}
///</DXSTEREO>

///<DXSTEREO> Procesamiento del componente de normales
      array = (Array)DXGetComponentValue(field, "normals");
      if (! array)
	{
	  DXSetError(ERROR_BAD_CLASS, "\"obj\" contains no normals component");
	  goto error;
	}

      DXGetArrayInfo(array, &norm_knt, NULL, NULL, NULL, &norm_dim);

      n_normals = (float *)DXGetArrayData(array);
      if (! n_normals)
	goto error;
///</DXSTEREO>

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
      in_knt[0] = (int) NULL;
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
   * Call the user's routine.  Check the return code.
   */
#ifdef DXSTEREO_DEBUG
  DXMessage("Calling worker!");
#endif
  result = dxstereo_worker(
			   p_knt, p_dim, (float *)p_positions,
			   c_knt, c_nv, (int *)c_connections,
			   col_knt, col_dim, (float *)c_colors,
			   opa_knt, opa_dim, (float *)o_opacities,
			   norm_knt, norm_dim, (float *)n_normals,
			   in_knt[0], (float *)in_data[0]);
#ifdef DXSTEREO_DEBUG
  DXMessage("Worker worked!");
#endif

  if (! result)
    if (DXGetError()==ERROR_NONE)
      DXSetError(ERROR_INTERNAL, "error return from user routine");

  /*
   * In either event, clean up allocated memory
   */

 error:
  return result;
}

//Esta funcion es invocada por doLeaf con los datos de posiciones, conexiones, colores, normales y datos
int dxstereo_worker(
		int p_knt, int p_dim, float *p_positions,
		int c_knt, int c_nv, int *c_connections,
		int col_knt, int col_dim, float *c_colors,
		int opa_knt, int opa_dim, float *o_opacities,
		int norm_knt, int norm_dim, float *n_normals,
		int obj_knt, float *obj_data)
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
   *
      */

 ///<DXSTEREO> Variables utilizadas en la extraccion de los datos
 //           y la creacion de las listas de despliegue.
  int vector_index,p_index,c_index,shape;
  float x,y,z;
  GLfloat material[4];

#ifdef DXSTEREO_DEBUG
  DXMessage("Worker working!");
#endif

 //Falla si los datos no son tridimensionales
//  if (p_dim != 3)
//    goto error;

  if (p_dim == 3) {
 //Fija los valores iniciales para las aristas de la caja de datos
  minX=maxX=p_positions[0];
  minY=maxY=p_positions[1];
  minZ=maxZ=p_positions[2];

 //Determina las conexiones de los datos
  if (c_nv <= 1)
    shape = GL_POINTS;
  else if (c_nv == 2)
    shape = GL_LINE_STRIP;
  else if (c_nv == 3)
    shape = GL_TRIANGLE_STRIP;
  else if (c_nv == 4)
    shape = GL_QUAD_STRIP;
  else if (c_nv > 3)
    shape = GL_POLYGON;

 //Crea una nueva lista de despliegue de OpenGL
  glNewList(++dlist_knt, GL_COMPILE);

#ifdef DXSTEREO_DEBUG
  sprintf(strerr,"Creating new display list of shape (%d): %i\n", c_nv, dlist_knt);
  writeLog(strerr);
#endif

 //Extrae las posiciones de cada elemento y las introduce en una lista de despliegue
  for (c_index=0;c_index<c_knt;c_index++)
    {
 //Inicia el bloque de instrucciuones OpenGL
      glBegin(shape);
      for (vector_index=0;vector_index<c_nv;vector_index++)
	{
 //Extrae las coordenadas de un punto
	  p_index = c_connections[c_index*c_nv+vector_index];
	  x = p_positions[p_index*3];
	  y = p_positions[p_index*3+1];
	  z = p_positions[p_index*3+2];

#ifdef DXSTEREO_DEBUG
	  sprintf(strerr,"C: (%.3f,%.3f,%.3f)\n", c_colors[p_index*3], c_colors[p_index*3+1],
		  c_colors[p_index*3+2]);
	  writeLog(strerr);
	  sprintf(strerr,"N: (%.3f,%.3f,%.3f)\n", n_normals[p_index*3], n_normals[p_index*3+1],
		  n_normals[p_index*3+2]);
	  writeLog(strerr);
#endif
 //Define la normal
	  glNormal3fv(&n_normals[p_index*3]);
#ifdef DXSTEREO_DEBUG
	  sprintf(strerr,"V: (%.3f,%.3f,%.3f)\n", x, y, z);
	  writeLog(strerr);
#endif
 //Determina el material
	  material[0] = (GLfloat) c_colors[p_index*3];
	  material[1] = (GLfloat) c_colors[p_index*3+1];
	  material[2] = (GLfloat) c_colors[p_index*3+2];
//Utiliza los valores de opacidad extraidos o 1.0 si no se ha especificado de otra manera
	  if (opa_knt && p_index < opa_knt)
	    material[3] = (GLfloat) o_opacities[p_index];
	  else
	    material[3] = (GLfloat) 1.0;
 //Define el material
 	  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material);
#ifdef DXSTEREO_DEBUG
	  sprintf(strerr,"M: (%.3f,%.3f,%.3f,%.3f)\n", material[0], material[1], material[2], material[3]);
	  writeLog(strerr);
#endif
 //Define el vertice
	  glVertex3fv(&p_positions[p_index*3]);
 //Ajusta los limites de la caja
	  if (x<minX || x>maxX)
	    {
	      if (x<minX)
		minX = x;
	      else
		maxX = x;
	    }
	  if (y<minY || y>maxY)
	    {
	      if (y<minY)
		minY = y;
	      else
		maxY = y;
	    }
	  if (z<minZ || z>maxZ)
	    {
	      if (z<minZ)
		minZ = z;
	      else
		maxZ = z;
	    }
	}
 //Termina el bloque OpenGL
      glEnd();
    }

 //Termina la definicion de la lista de despliegue
  glEndList();

#ifdef DXSTEREO_DEBUG
  //Checa por errores de OpenGL
  checkError("worker");
#endif

  /*
   * successful completion
   */
  return 1;

  }
  /*
   * unsuccessful completion
   */
 error:
  return 0;

}

//Fija el volumen de vision y la perspectiva
void setFrustum(void)
{
    double top = nearZ*tan(DTR*fovy/2); //Calcula la parte superior del frustum basandose en los valores de fovy y la posicion del plano cercano de corte
    double right = aspect*top; //Calcula el lado derecho del frustum basado en el valor de aspecto
    double frustumshift = (IOD/2)*nearZ/screenZ; //Calcula el desplazamiento para crear el frustum asimetrico, se basa en el plano cercano de corte y en el plano de proyeccion

    //Fija los valores para la camara izquierda
    leftCam.topfrustum = top;
    leftCam.bottomfrustum = -top;
    leftCam.leftfrustum = -right + frustumshift;
    leftCam.rightfrustum = right + frustumshift;
    leftCam.modeltranslation = IOD/2;

    //Fija los valores para la camara derecha
    rightCam.topfrustum = top;
    rightCam.bottomfrustum = -top;
    rightCam.leftfrustum = -right - frustumshift;
    rightCam.rightfrustum = right - frustumshift;
    rightCam.modeltranslation = -IOD/2;
}

//Inicializa el estado de los buffers
void init(void)
{
  glViewport (0, 0, width, height); //Establece el viewport a toda la ventana
  //Inicializa la matriz de proyeccion
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  //Inicializa la matriz del modelo
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

//Maneja cambios en la forma de la ventana
GLvoid reshape(GLsizei w, GLsizei h)
{
    if (h==0)
    {
        h=1; //evita la division por cero
    }
    aspect=(double)w/(double)h; //Calcula la razon de aspecto
    glViewport(0, 0, w, h); //Establece el viewport a toda la ventana
    setFrustum(); //Ajusta los valores del frustum para ambas camaras
}

//Administra los buffers para desplegar una sola imagen
GLvoid display_mono(GLvoid)
{
  glDrawBuffer(GL_BACK); //Dibuja en ambos buffers de fondo
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Limpia los buffers de color y profundidad

  //Reinicia la matriz de proyeccion
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  //Establece el volumen de vision usando la camara izquierda
  glFrustum(leftCam.leftfrustum, leftCam.rightfrustum,
            leftCam.bottomfrustum, leftCam.topfrustum,
            nearZ, farZ);
  glTranslatef(leftCam.modeltranslation, 0.0, 0.0); //Translada para cancelar el paralelaje
  //Reinicia la matriz de modelo
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glPushMatrix();
  {
    glTranslatef(0.0, 0.0, depthZ); //Translada al plano de proyeccion
    polarView(distance, azimAngle, incAngle, twistAngle); //Desplaza y rota el objeto de acuerdo a los movimientos del raton
    glTranslatef(-center[0], -center[1], -center[2]); //Centra el objeto en el volumen de vision
    drawScene(); //Ejecuta las listas de despliegue
  }
  glPopMatrix();

  glutSwapBuffers(); //Intercambia los buffers trasero y delantero

#ifdef DXSTEREO_DEBUG
  //Checa por errores de OpenGL
  checkError("display_mono");
#endif
}

//Administra los buffers para desplegar usando quad-buffering
GLvoid display_stereo(GLvoid)
{
  glDrawBuffer(GL_BACK_LEFT); //Dibuja en el buffer trasero izquierdo
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Limpia los buffers de color y profundidad

  //Reinicia la matriz de proyeccion
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  //Establece el volumen de vision usando la camara izquierda
  glFrustum(leftCam.leftfrustum, leftCam.rightfrustum,
            leftCam.bottomfrustum, leftCam.topfrustum,
            nearZ, farZ);
  glTranslatef(leftCam.modeltranslation, 0.0, 0.0); //Translada para cancelar el paralelaje
  //Reinicia la matriz de modelo
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glPushMatrix();
  {
    glTranslatef(0.0, 0.0, depthZ); //Translada al plano de proyeccion
    polarView(distance, azimAngle, incAngle, twistAngle); //Desplaza y rota el objeto de acuerdo a los movimientos del raton
    glTranslatef(-center[0], -center[1], -center[2]); //Centra el objeto en el volumen de vision
    drawScene(); //Ejecuta las listas de despliegue
  }
  glPopMatrix();

  glDrawBuffer(GL_BACK_RIGHT); //Dibuja en el buffer trasero derecho
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Limpia los buffers de color y profundidad

  //Reinicia la matriz de proyeccion
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  //Establece el volumen de vision usando la camara derecha
  glFrustum(rightCam.leftfrustum, rightCam.rightfrustum,
            rightCam.bottomfrustum, rightCam.topfrustum,
            nearZ, farZ);
  glTranslatef(rightCam.modeltranslation, 0.0, 0.0); //Translada para cancelar el paralelaje
  //Reinicia la matriz de modelo
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glPushMatrix();
  {
    glTranslatef(0.0, 0.0, depthZ); //Translada al plano de proyeccion
    polarView(distance, azimAngle, incAngle, twistAngle); //Desplaza y rota el objeto de acuerdo a los movimientos del raton
    glTranslatef(-center[0], -center[1], -center[2]); //Centra el objeto en el volumen de vision
    drawScene(); //Ejecuta las listas de despliegue
  }
  glPopMatrix();

  glutSwapBuffers(); //Intercambia los buffers trasero y delantero

#ifdef DXSTEREO_DEBUG
  //Checa por errores de OpenGL
  checkError("display_stereo");
#endif
}

//Ejecuta las listas de despliegue
void drawScene(void)
{
  int dlist_index; //Indice de las listas de despliegue

  for (dlist_index=1;dlist_index<=dlist_knt;dlist_index++)
    {
      glCallList(dlist_index); //Llama una lista a la vez, en el orden en que fueron definidas (corresponde al orden de los objetos en el arbol de objetos)
    }
}

//Calcula la distancia entre dos punto en un espacio 3D
float dist(float *p1, float *p2)
{
  float origin[] = {0.0, 0.0, 0.0};
  float d, dx, dy, dz;

  if (p2 == NULL)
    {
      p2 = origin;
    }

  dx=p1[0]-p2[0];
  dy=p1[1]-p2[1];
  dz=p1[2]-p2[2];
  d=sqrt(dx*dx+dy*dy+dz*dz);
  return d;
}

//Maneja eventos del teclado
GLvoid keyboard(GLubyte key, GLint x, GLint y)
{
  switch (key)
    {
    //Modifica la distancia intraocular
    case 'I':
      IOD += 0.01;
      setFrustum();
      glutPostRedisplay();
      break;
    case 'i':
      IOD -= 0.01;
      setFrustum();
      glutPostRedisplay();
      break;
    //Activa el despliegue en pantalla completa
    case 'f':
    case 'F':
      glutFullScreen();
      break;
    //Restablece el desplazamiento y rotacion del objeto
    case 'r':
    case 'R':
      resetView();
      glutPostRedisplay();
      break;
    //Termina DXStereo
    case 'q':
    case 'Q':
#ifdef FREEGLUT
      glutLeaveMainLoop();
#else
      exit(0);
#endif
      break;
    }
}

//Restablece los valores del desplazamiento y rotacion del objeto
void resetView( GLvoid )
{
	distance = 0.0;
	twistAngle = 0.0;
	incAngle = 0.0;
	azimAngle = 0.0;
	fovy = 45.0;
}

//Translada y rota el objeto de acuerdo a los movimientos del raton
void polarView( GLfloat distance, GLfloat azimuth, GLfloat incidence,
			GLfloat twist)
{
	glTranslatef( 0.0, 0.0, -distance);
	glRotatef( -twist, 0.0, 0.0, 1.0);
	glRotatef( -incidence, 1.0, 0.0, 0.0);
	glRotatef( -azimuth, 0.0, 0.0, 1.0);
}

//Maneja eventos del raton
GLvoid mouse( GLint button, GLint state, GLint x, GLint y )
{
	static GLint buttons_down = 0;

	if (state == GLUT_DOWN) {
		switch (button) {
		case GLUT_LEFT_BUTTON:
			action = MOVE_EYE;
			break;
		case GLUT_MIDDLE_BUTTON:
			action = TWIST_EYE;
			break;
		case GLUT_RIGHT_BUTTON:
			action = ZOOM;
			break;
		}

		xStart = x;
		yStart = y;
	} else {
		if (--buttons_down == 0)
			action = MOVE_NONE;
	}

}

//Maneja eventos de desplazamiento del raton
GLvoid motion( GLint x, GLint y )
{
	switch (action) {
	case MOVE_EYE:
		//Ajusta la posicion del ojo
		azimAngle += (GLdouble) (x - xStart);
		incAngle -= (GLdouble) (y - yStart);
		break;
	case TWIST_EYE:
		//Ajusta el giro del ojo
		twistAngle = fmodf(twistAngle+(x - xStart), 360.0);
		break;
	case ZOOM:
		//Ajusta la distancia al ojo
		distance -= (GLdouble) (y - yStart)/10.0;
		break;
	default:
		break;
	}

	xStart = x;
	yStart = y;

	glutPostRedisplay();
}

#ifdef DXSTEREO_DEBUG
//Detecta errores de OpenGL
void checkError(char *label)
{
  GLenum error;

  while ((error = glGetError()) != GL_NO_ERROR)
    {
      sprintf(strerr, "%s: %s\n", label, gluErrorString(error) );
      writeLog(strerr);
      DXMessage(gluErrorString(error));
    }
}

//Usada si se compila con la bandera DXSTEREO_DEBUG
void writeLog(char * message)
{
  FILE *fp;

  fp = fopen("log", "a");
  fprintf(fp, message);
  fclose(fp);
}
#endif
