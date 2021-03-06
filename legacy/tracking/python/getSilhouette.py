# -*- coding: utf-8 -*-
import os
import pdb
import numpy
import sys
from numpy import cos, pi, sin

numpy.set_printoptions(precision=4)
if os.environ.get('OSTYPE') == 'linux':
    directory = '/windows/D/Research/modelbasedtracking'
else: 
    directory = 'D:\Research\modelbasedtracking'

os.chdir(directory + '/models')
modelname = 'LND_3.lwo'
basename = os.path.splitext(modelname)[0]

filePolys = open(directory + '/models/' + basename + '.pols')
fileVerts = open(directory + '/models/' + basename + '.verts')
fileNorms = open(directory + '/models/' + basename + '.poly_norms')
fileBarycenters = open(directory + '/models/' + basename + '.barycenters')

# in the future, read transformation matrix in as argument
R0 = [[1,0,0],[0,cos(pi/2),-sin(pi/2)],[0,sin(pi/2),cos(pi/2)]];
R1 = [[cos(-2*pi/3),0,-sin(-2*pi/3)],[0,1,0],[sin(-2*pi/3),0,cos(-2*pi/3)]];
R2 = [[cos(pi/4),-sin(pi/4),0],[sin(pi/4),cos(pi/4),0],[0,0,1]];
R0 = numpy.matrix(R0); R1 = numpy.matrix(R1); R2 = numpy.matrix(R2)
R = R2 * R1 * R0;
t = [0, 90, 70];
t = numpy.matrix(t).transpose() # t = t.transpose()
T = numpy.zeros((4,4))
T = numpy.matrix(T)
T[0:3, 0:3] = R
T[0:3, 3] = t
T[3, 3] = 1
print "transformation parsed"

####################### read files #############################
verts = [[float(val) for val in line.rstrip().split(' ')] for line in fileVerts]
verts = numpy.matrix(verts)
polys = [tuple([int(val) for val in line.rstrip().split(' ')]) for line in filePolys]
norms = [[float(val) for val in line.rstrip().split(' ')] for line in fileNorms]
norms = numpy.matrix(norms)
barycenters = [[float(val) for val in line.rstrip().split(' ')] for line in fileBarycenters]
barycenters = numpy.matrix(barycenters)

filePolys.close()
fileVerts.close()
fileNorms.close()
fileBarycenters.close()

####################### apply transform ########################
homogeneousVerts = numpy.vstack([verts.transpose(), numpy.ones((1,4))])
print homogeneousVerts[:, 0:10]
transformedHomVerts = T * homogeneousVerts
transformedVerts = transformedHomVerts[0:3, :]

