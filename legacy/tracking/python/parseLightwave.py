# -*- coding: utf-8 -*-
import os
import struct
import pdb

if os.environ.get('OSTYPE') == 'linux':
	directory = '/windows/D/Research/modelbasedtracking'
else: 
	directory = 'D:\Research\modelbasedtracking'
	
os.chdir(directory + '/models')
modelname = 'inner_shaft.lwo'


def read_vec(vec):
    return struct.unpack('>fff', vec)


f = open(modelname, 'rb')
form = f.read(4)
print form
fileLen_raw = f.read(4)
fileLen = struct.unpack('>i', fileLen_raw)
fileLen = fileLen[0]
print 'fileLen', fileLen
lwob = f.read(4)
print lwob

marker = 4; # 4 for LWOB
while marker < fileLen:
    tag = f.read(4)
    print tag
    datasize_raw = f.read(4)
    datasize = struct.unpack('>i', datasize_raw)
    datasize = datasize[0]
    print 'datasize', datasize

    if datasize%2 == 1:
        data = f.read(datasize+1)
    else:
        data = f.read(datasize)

    if tag == 'PNTS':
        vertices = []
        numPts = datasize/12
        print 'numPts', numPts
        for i in range(numPts):
            vec = data[i*12:(i+1)*12]
            (x,y,z) = read_vec(vec)
            vertices.append([x,y,z])
            
            if False:
                print 'vec', i
                print (x,y,z)
    elif tag == 'POLS':
        polyType = data[0:4]
        print "polyType", polyType

        polygons = []
        polyMarker = 4
        polyCounter = 0
        while polyMarker < datasize:
            polyCounter += 1
            
            numvertAndFlags = struct.unpack('>H', data[polyMarker:polyMarker+2])
            numvertAndFlags = numvertAndFlags[0]
            first6Bits = numvertAndFlags & int('fc00', 16)        # mask out all but the first 6 bits
            flags = first6Bits >> 10                              # bitshift zeros out
            numVerts = numvertAndFlags & int('3ff', 16)           # mask out first 6 bits

            ### get vertex indices of polygon ###            
            polyMarker = polyMarker + 2
            indices = []
            for i in range(numVerts):
                lengthFlag = data[polyMarker:polyMarker+2]
                longForm = (lengthFlag == '\xff')             # variable-length index. If the first byte is less than FF, then the index is only 2 bytes long. If it is FF, then four bytes are used, with the first flag byte being masked out (to provide a range from 0 to FFFFFF) 

                if not longForm:    # read as a 2-byte unsigned int between 0 and FF00
                    index = struct.unpack('>H', data[polyMarker:polyMarker+2])
                    polyMarker +=2
                else:               # read as a 4-byte unsigned int between 0 and FFFFFF (only 3 significant bytes since first one is flag)
                    index = struct.unpack('>I', data[polyMarker:polyMarker+4])
                    index = index & int('ffffff', 16)             # mask out first byte (8 bits)
                    polyMarker += 4
                index = index[0]+1			# index vertices from 1
                indices.append(index)
            # endfor loop over polygon vertices
            polygons.append(indices)

            ### output helper ###
            if False:
                flagsBinaryString = "";
                for i in range(6):
                    flagsBinaryString = flagsBinaryString + str((flags >> (6-i-1))%2)
                numVertsBinaryString = "";
                for i in range(10):
                    numVertsBinaryString = numVertsBinaryString + str((numVerts >> (10-i-1))%2)
                print "numvertAndFlags", hex(numvertAndFlags), ": ", "flags", flagsBinaryString, "flags", flags
                print "numVerts", numVerts, "numVerts", numVertsBinaryString
                print "Polygon", polyCounter, [vertices[i] for i in indices]

            # endfor loop over polygons
        # endif POLS
        print "Number polygons read:", polyCounter
        print "polyMarker/datasize:", polyMarker, datasize
    
    marker = f.tell() - 8
# end while for file read
f.close()


### switch y and z so z is height ###
for v in vertices:
    tmp = v[2]
    v[2] = v[1]
    v[1] = tmp

basename = os.path.splitext(modelname)[0]
outfileVerts = open(directory + '/models/' + basename + '.verts', 'w')
for v in vertices:
    vstr = " ".join([str(val) for val in v])
    outfileVerts.write(vstr + '\n')
outfileVerts.close()
print outfileVerts.name, 'written'

outfilePolys = open(directory + '/models/' + basename + '.pols', 'w')
for p in polygons:
    pstr = " ".join([str(val) for val in p])
    outfilePolys.write(pstr + '\n')
outfilePolys.close()
print outfilePolys.name, 'written'



