#ifndef _ObjectTrajectoryRecorder_h
#define _ObjectTrajectoryRecorder_h

#include <stdlib.h>

#ifdef MAC
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <GL/gle.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/gle.h>
#endif

#include <iostream>
#include <fstream>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <math.h>

#include "EnvObjects/World.h"
#include "ThreadConstrained.h"
#include "EnvObjects/EnvObject.h"
#include "ObjectTrajectoryReader.h"

// import most common Eigen types
USING_PART_OF_NAMESPACE_EIGEN

class ObjectTrajectoryRecorder
{
	public:
		ObjectTrajectoryRecorder();
		ObjectTrajectoryRecorder(const char* fileName);
    void setFileName(const char* newFileName);

		void writeObjectsToFile(World* world);

	private:
		char _fileName[256];		
};

#endif
