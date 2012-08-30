import sys, os
import time
end_time = time.time()
start_time = 0
mpi_avail = True
try:
	importmpi_start = time.time()
	import mpi
	importmpi_end = time.time()
	importmpi_time = importmpi_end - importmpi_start
except:
	mpi_avail = False
if mpi_avail == False:
	print 'Sequoia Benchmark Version 1.1.0\n'
	if len(sys.argv) > 1:
		start_time = float(sys.argv[1])
		print 'startup time = ' + str(end_time - start_time) + ' secs'
	print 'pynamic driver beginning... now importing modules'
else:
	if mpi.rank == 0:
		print 'Sequoia Benchmark Version 1.1.0\n'
		if len(sys.argv) > 1:
			start_time = float(sys.argv[1])
			print 'Pynamic: call  time = %10.6f\n' % start_time
			print 'Pynamic: start time = %10.6f\n' % end_time
			print 'startup time = ' + str(end_time - start_time) + ' secs'
		print 'pynamic driver beginning... now importing modules'
import_start = time.time()
import libmodule0
import libmodule1
import libmodule2
import libmodule3
import libmodule4
import libmodule5
import libmodule6
import libmodule7
import libmodule8
import libmodule9
import libmodule10
import libmodule11
import libmodule12
import libmodule13
import libmodule14
import libmodule15
import libmodule16
import libmodule17
import libmodule18
import libmodule19
import libmodule20
import libmodule21
import libmodule22
import libmodule23
import libmodule24
import libmodule25
import libmodule26
import libmodule27
import libmodule28
import libmodule29
import libmodule30
import libmodule31
import libmodule32
import libmodule33
import libmodule34
import libmodule35
import libmodule36
import libmodule37
import libmodule38
import libmodule39
import libmodule40
import libmodule41
import libmodule42
import libmodule43
import libmodule44
import libmodule45
import libmodule46
import libmodule47
import libmodule48
import libmodule49
import libmodule50
import libmodule51
import libmodule52
import libmodule53
import libmodule54
import libmodule55
import libmodule56
import libmodule57
import libmodule58
import libmodule59
import libmodule60
import libmodule61
import libmodule62
import libmodule63
import libmodule64
import libmodule65
import libmodule66
import libmodule67
import libmodule68
import libmodule69
import libmodule70
import libmodule71
import libmodule72
import libmodule73
import libmodule74
import libmodule75
import libmodule76
import libmodule77
import libmodule78
import libmodule79
import libmodule80
import libmodule81
import libmodule82
import libmodule83
import libmodule84
import libmodule85
import libmodule86
import libmodule87
import libmodule88
import libmodule89
import libmodule90
import libmodule91
import libmodule92
import libmodule93
import libmodule94
import libmodule95
import libmodule96
import libmodule97
import libmodule98
import libmodule99
import libmodule100
import libmodule101
import libmodule102
import libmodule103
import libmodule104
import libmodule105
import libmodule106
import libmodule107
import libmodule108
import libmodule109
import libmodule110
import libmodule111
import libmodule112
import libmodule113
import libmodule114
import libmodule115
import libmodule116
import libmodule117
import libmodule118
import libmodule119
import libmodule120
import libmodule121
import libmodule122
import libmodule123
import libmodule124
import libmodule125
import libmodule126
import libmodule127
import libmodule128
import libmodule129
import libmodule130
import libmodule131
import libmodule132
import libmodule133
import libmodule134
import libmodule135
import libmodule136
import libmodule137
import libmodule138
import libmodule139
import libmodule140
import libmodule141
import libmodule142
import libmodule143
import libmodule144
import libmodule145
import libmodule146
import libmodule147
import libmodule148
import libmodule149
import libmodule150
import libmodule151
import libmodule152
import libmodule153
import libmodule154
import libmodule155
import libmodule156
import libmodule157
import libmodule158
import libmodule159
import libmodule160
import libmodule161
import libmodule162
import libmodule163
import libmodule164
import libmodule165
import libmodule166
import libmodule167
import libmodule168
import libmodule169
import libmodule170
import libmodule171
import libmodule172
import libmodule173
import libmodule174
import libmodule175
import libmodule176
import libmodule177
import libmodule178
import libmodule179
import libmodule180
import libmodule181
import libmodule182
import libmodule183
import libmodule184
import libmodule185
import libmodule186
import libmodule187
import libmodule188
import libmodule189
import libmodule190
import libmodule191
import libmodule192
import libmodule193
import libmodule194
import libmodule195
import libmodule196
import libmodule197
import libmodule198
import libmodule199
import libmodule200
import libmodule201
import libmodule202
import libmodule203
import libmodule204
import libmodule205
import libmodule206
import libmodule207
import libmodule208
import libmodule209
import libmodule210
import libmodule211
import libmodule212
import libmodule213
import libmodule214
import libmodule215
import libmodule216
import libmodule217
import libmodule218
import libmodule219
import libmodule220
import libmodule221
import libmodule222
import libmodule223
import libmodule224
import libmodule225
import libmodule226
import libmodule227
import libmodule228
import libmodule229
import libmodule230
import libmodule231
import libmodule232
import libmodule233
import libmodule234
import libmodule235
import libmodule236
import libmodule237
import libmodule238
import libmodule239
import libmodule240
import libmodule241
import libmodule242
import libmodule243
import libmodule244
import libmodule245
import libmodule246
import libmodule247
import libmodule248
import libmodule249
import libmodule250
import libmodule251
import libmodule252
import libmodule253
import libmodule254
import libmodule255
import libmodule256
import libmodule257
import libmodule258
import libmodule259
import libmodule260
import libmodule261
import libmodule262
import libmodule263
import libmodule264
import libmodule265
import libmodule266
import libmodule267
import libmodule268
import libmodule269
import libmodule270
import libmodule271
import libmodule272
import libmodule273
import libmodule274
import libmodule275
import libmodule276
import libmodule277
import libmodule278
import libmodule279
import_end = time.time()
import_time = import_end - import_start
if mpi_avail == False:
	print 'pynamic driver finished importing all modules... visiting all module functions'
else:
	if mpi.rank == 0:
		print 'pynamic driver finished importing all modules... visiting all module functions'
call_start = time.time()
libmodule0.libmodule0_entry()
libmodule1.libmodule1_entry()
libmodule2.libmodule2_entry()
libmodule3.libmodule3_entry()
libmodule4.libmodule4_entry()
libmodule5.libmodule5_entry()
libmodule6.libmodule6_entry()
libmodule7.libmodule7_entry()
libmodule8.libmodule8_entry()
libmodule9.libmodule9_entry()
libmodule10.libmodule10_entry()
libmodule11.libmodule11_entry()
libmodule12.libmodule12_entry()
libmodule13.libmodule13_entry()
libmodule14.libmodule14_entry()
libmodule15.libmodule15_entry()
libmodule16.libmodule16_entry()
libmodule17.libmodule17_entry()
libmodule18.libmodule18_entry()
libmodule19.libmodule19_entry()
libmodule20.libmodule20_entry()
libmodule21.libmodule21_entry()
libmodule22.libmodule22_entry()
libmodule23.libmodule23_entry()
libmodule24.libmodule24_entry()
libmodule25.libmodule25_entry()
libmodule26.libmodule26_entry()
libmodule27.libmodule27_entry()
libmodule28.libmodule28_entry()
libmodule29.libmodule29_entry()
libmodule30.libmodule30_entry()
libmodule31.libmodule31_entry()
libmodule32.libmodule32_entry()
libmodule33.libmodule33_entry()
libmodule34.libmodule34_entry()
libmodule35.libmodule35_entry()
libmodule36.libmodule36_entry()
libmodule37.libmodule37_entry()
libmodule38.libmodule38_entry()
libmodule39.libmodule39_entry()
libmodule40.libmodule40_entry()
libmodule41.libmodule41_entry()
libmodule42.libmodule42_entry()
libmodule43.libmodule43_entry()
libmodule44.libmodule44_entry()
libmodule45.libmodule45_entry()
libmodule46.libmodule46_entry()
libmodule47.libmodule47_entry()
libmodule48.libmodule48_entry()
libmodule49.libmodule49_entry()
libmodule50.libmodule50_entry()
libmodule51.libmodule51_entry()
libmodule52.libmodule52_entry()
libmodule53.libmodule53_entry()
libmodule54.libmodule54_entry()
libmodule55.libmodule55_entry()
libmodule56.libmodule56_entry()
libmodule57.libmodule57_entry()
libmodule58.libmodule58_entry()
libmodule59.libmodule59_entry()
libmodule60.libmodule60_entry()
libmodule61.libmodule61_entry()
libmodule62.libmodule62_entry()
libmodule63.libmodule63_entry()
libmodule64.libmodule64_entry()
libmodule65.libmodule65_entry()
libmodule66.libmodule66_entry()
libmodule67.libmodule67_entry()
libmodule68.libmodule68_entry()
libmodule69.libmodule69_entry()
libmodule70.libmodule70_entry()
libmodule71.libmodule71_entry()
libmodule72.libmodule72_entry()
libmodule73.libmodule73_entry()
libmodule74.libmodule74_entry()
libmodule75.libmodule75_entry()
libmodule76.libmodule76_entry()
libmodule77.libmodule77_entry()
libmodule78.libmodule78_entry()
libmodule79.libmodule79_entry()
libmodule80.libmodule80_entry()
libmodule81.libmodule81_entry()
libmodule82.libmodule82_entry()
libmodule83.libmodule83_entry()
libmodule84.libmodule84_entry()
libmodule85.libmodule85_entry()
libmodule86.libmodule86_entry()
libmodule87.libmodule87_entry()
libmodule88.libmodule88_entry()
libmodule89.libmodule89_entry()
libmodule90.libmodule90_entry()
libmodule91.libmodule91_entry()
libmodule92.libmodule92_entry()
libmodule93.libmodule93_entry()
libmodule94.libmodule94_entry()
libmodule95.libmodule95_entry()
libmodule96.libmodule96_entry()
libmodule97.libmodule97_entry()
libmodule98.libmodule98_entry()
libmodule99.libmodule99_entry()
libmodule100.libmodule100_entry()
libmodule101.libmodule101_entry()
libmodule102.libmodule102_entry()
libmodule103.libmodule103_entry()
libmodule104.libmodule104_entry()
libmodule105.libmodule105_entry()
libmodule106.libmodule106_entry()
libmodule107.libmodule107_entry()
libmodule108.libmodule108_entry()
libmodule109.libmodule109_entry()
libmodule110.libmodule110_entry()
libmodule111.libmodule111_entry()
libmodule112.libmodule112_entry()
libmodule113.libmodule113_entry()
libmodule114.libmodule114_entry()
libmodule115.libmodule115_entry()
libmodule116.libmodule116_entry()
libmodule117.libmodule117_entry()
libmodule118.libmodule118_entry()
libmodule119.libmodule119_entry()
libmodule120.libmodule120_entry()
libmodule121.libmodule121_entry()
libmodule122.libmodule122_entry()
libmodule123.libmodule123_entry()
libmodule124.libmodule124_entry()
libmodule125.libmodule125_entry()
libmodule126.libmodule126_entry()
libmodule127.libmodule127_entry()
libmodule128.libmodule128_entry()
libmodule129.libmodule129_entry()
libmodule130.libmodule130_entry()
libmodule131.libmodule131_entry()
libmodule132.libmodule132_entry()
libmodule133.libmodule133_entry()
libmodule134.libmodule134_entry()
libmodule135.libmodule135_entry()
libmodule136.libmodule136_entry()
libmodule137.libmodule137_entry()
libmodule138.libmodule138_entry()
libmodule139.libmodule139_entry()
libmodule140.libmodule140_entry()
libmodule141.libmodule141_entry()
libmodule142.libmodule142_entry()
libmodule143.libmodule143_entry()
libmodule144.libmodule144_entry()
libmodule145.libmodule145_entry()
libmodule146.libmodule146_entry()
libmodule147.libmodule147_entry()
libmodule148.libmodule148_entry()
libmodule149.libmodule149_entry()
libmodule150.libmodule150_entry()
libmodule151.libmodule151_entry()
libmodule152.libmodule152_entry()
libmodule153.libmodule153_entry()
libmodule154.libmodule154_entry()
libmodule155.libmodule155_entry()
libmodule156.libmodule156_entry()
libmodule157.libmodule157_entry()
libmodule158.libmodule158_entry()
libmodule159.libmodule159_entry()
libmodule160.libmodule160_entry()
libmodule161.libmodule161_entry()
libmodule162.libmodule162_entry()
libmodule163.libmodule163_entry()
libmodule164.libmodule164_entry()
libmodule165.libmodule165_entry()
libmodule166.libmodule166_entry()
libmodule167.libmodule167_entry()
libmodule168.libmodule168_entry()
libmodule169.libmodule169_entry()
libmodule170.libmodule170_entry()
libmodule171.libmodule171_entry()
libmodule172.libmodule172_entry()
libmodule173.libmodule173_entry()
libmodule174.libmodule174_entry()
libmodule175.libmodule175_entry()
libmodule176.libmodule176_entry()
libmodule177.libmodule177_entry()
libmodule178.libmodule178_entry()
libmodule179.libmodule179_entry()
libmodule180.libmodule180_entry()
libmodule181.libmodule181_entry()
libmodule182.libmodule182_entry()
libmodule183.libmodule183_entry()
libmodule184.libmodule184_entry()
libmodule185.libmodule185_entry()
libmodule186.libmodule186_entry()
libmodule187.libmodule187_entry()
libmodule188.libmodule188_entry()
libmodule189.libmodule189_entry()
libmodule190.libmodule190_entry()
libmodule191.libmodule191_entry()
libmodule192.libmodule192_entry()
libmodule193.libmodule193_entry()
libmodule194.libmodule194_entry()
libmodule195.libmodule195_entry()
libmodule196.libmodule196_entry()
libmodule197.libmodule197_entry()
libmodule198.libmodule198_entry()
libmodule199.libmodule199_entry()
libmodule200.libmodule200_entry()
libmodule201.libmodule201_entry()
libmodule202.libmodule202_entry()
libmodule203.libmodule203_entry()
libmodule204.libmodule204_entry()
libmodule205.libmodule205_entry()
libmodule206.libmodule206_entry()
libmodule207.libmodule207_entry()
libmodule208.libmodule208_entry()
libmodule209.libmodule209_entry()
libmodule210.libmodule210_entry()
libmodule211.libmodule211_entry()
libmodule212.libmodule212_entry()
libmodule213.libmodule213_entry()
libmodule214.libmodule214_entry()
libmodule215.libmodule215_entry()
libmodule216.libmodule216_entry()
libmodule217.libmodule217_entry()
libmodule218.libmodule218_entry()
libmodule219.libmodule219_entry()
libmodule220.libmodule220_entry()
libmodule221.libmodule221_entry()
libmodule222.libmodule222_entry()
libmodule223.libmodule223_entry()
libmodule224.libmodule224_entry()
libmodule225.libmodule225_entry()
libmodule226.libmodule226_entry()
libmodule227.libmodule227_entry()
libmodule228.libmodule228_entry()
libmodule229.libmodule229_entry()
libmodule230.libmodule230_entry()
libmodule231.libmodule231_entry()
libmodule232.libmodule232_entry()
libmodule233.libmodule233_entry()
libmodule234.libmodule234_entry()
libmodule235.libmodule235_entry()
libmodule236.libmodule236_entry()
libmodule237.libmodule237_entry()
libmodule238.libmodule238_entry()
libmodule239.libmodule239_entry()
libmodule240.libmodule240_entry()
libmodule241.libmodule241_entry()
libmodule242.libmodule242_entry()
libmodule243.libmodule243_entry()
libmodule244.libmodule244_entry()
libmodule245.libmodule245_entry()
libmodule246.libmodule246_entry()
libmodule247.libmodule247_entry()
libmodule248.libmodule248_entry()
libmodule249.libmodule249_entry()
libmodule250.libmodule250_entry()
libmodule251.libmodule251_entry()
libmodule252.libmodule252_entry()
libmodule253.libmodule253_entry()
libmodule254.libmodule254_entry()
libmodule255.libmodule255_entry()
libmodule256.libmodule256_entry()
libmodule257.libmodule257_entry()
libmodule258.libmodule258_entry()
libmodule259.libmodule259_entry()
libmodule260.libmodule260_entry()
libmodule261.libmodule261_entry()
libmodule262.libmodule262_entry()
libmodule263.libmodule263_entry()
libmodule264.libmodule264_entry()
libmodule265.libmodule265_entry()
libmodule266.libmodule266_entry()
libmodule267.libmodule267_entry()
libmodule268.libmodule268_entry()
libmodule269.libmodule269_entry()
libmodule270.libmodule270_entry()
libmodule271.libmodule271_entry()
libmodule272.libmodule272_entry()
libmodule273.libmodule273_entry()
libmodule274.libmodule274_entry()
libmodule275.libmodule275_entry()
libmodule276.libmodule276_entry()
libmodule277.libmodule277_entry()
libmodule278.libmodule278_entry()
libmodule279.libmodule279_entry()
call_end = time.time()
call_time = call_end - call_start
if mpi_avail == False:
	print '\nPynamic: module import time = ' + str(import_time) + ' secs'
	print 'Pynamic: module visit time = ' + str(call_time) + ' secs'
	print 'Pynamic: module test passed!\n'
	sys.exit(0)
if mpi.rank == 0:
	print 'Pynamic: after import and visit time = %10.6f\n' % time.time()

importmpi_time_avg = mpi.reduce(importmpi_time, mpi.SUM)
importmpi_time_min = mpi.reduce(importmpi_time, mpi.MIN)
importmpi_time_max = mpi.reduce(importmpi_time, mpi.MAX)
import_time_avg = mpi.reduce(import_time, mpi.SUM)
import_time_min = mpi.reduce(import_time, mpi.MIN)
import_time_max = mpi.reduce(import_time, mpi.MAX)
call_time_avg = mpi.reduce(call_time, mpi.SUM)
call_time_min = mpi.reduce(call_time, mpi.MIN)
call_time_max = mpi.reduce(call_time, mpi.MAX)
if mpi.rank == 0:
	importmpi_time_avg = importmpi_time_avg / mpi.size
	import_time_avg = import_time_avg / mpi.size
	call_time_avg = call_time_avg / mpi.size
	print '\nPynamic: module importmpi time = ' + str(importmpi_time_avg) + ' secs'
	print 'Pynamic: module importmpi time = ' + str(importmpi_time_min) + ' secs (MIN)'
	print 'Pynamic: module importmpi time = ' + str(importmpi_time_max) + ' secs (MAX)'
	print 'Pynamic: module import time = ' + str(import_time_avg) + ' secs'
	print 'Pynamic: module import time = ' + str(import_time_min) + ' secs (MIN)'
	print 'Pynamic: module import time = ' + str(import_time_max) + ' secs (MAX)'
	print 'Pynamic: module visit time = ' + str(call_time_avg) + ' secs'
	print 'Pynamic: module visit time = ' + str(call_time_min) + ' secs (MIN)'
	print 'Pynamic: module visit time = ' + str(call_time_max) + ' secs (MAX)'
	print 'Pynamic: module test passed!\n'
	print 'Pynamic: testing mpi capability...\n'
	print 'Pynamic: after reduce time = %10.6f\n' % time.time()
mpi_start = time.time()
import mpi,sys
from array import *
from struct import *

#Function to return BMP header
def makeBMPHeader( width, height ):

    #Set up the bytes in the BMP header
    headerBytes = [ 66, 77, 28, 88, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0]
    headerBytes += [40] + 3*[0] + [100] + 3*[0] + [ 75,0,0,0,1,0,24] + [0]*9 + [18,11,0,0,18,11]
    headerBytes += [0]*10

    #Pack this data as a string
    data =""
    for x in range(54):
        data += pack( 'B', headerBytes[x] )
        
    #Create a string to overwrite the width and height in the BMP header
    replaceString = pack( '<I', width )
    replaceString += pack( '<I', height)
    
    #Return a 54-byte string that will be the new BMP header
    newString = data[0:18] + replaceString + data[26:54]
    return newString

#Define our fractal parameters
c = 0.4 + 0.3j
maxIterationsPerPoint = 64
distanceWhenUnbounded = 3.0

#define our function to iterate
def f( x ):
    return x*x + c

#Define the bounds of the xy plane we will work in
globalBounds = (-0.6, -0.6, 0.4, 0.4 ) #x1, y1, x2, y2

#define the size of the BMP to output
#For now this must be divisible by the # of processes
bmpSize = (400,400)

#Define the range of y-pixels in the BMP this process works on
myYPixelRange = [ 0,0]
myYPixelRange[0] = mpi.rank*bmpSize[1]/mpi.procs
myYPixelRange[1] = (mpi.rank+1)*bmpSize[1]/mpi.procs

if mpi.rank == 0:
    print "Starting computation (groan)\n"

#Now we can start to iterate over pixels!!
myString = ""
myArray = array('B')
for y in range( myYPixelRange[0], myYPixelRange[1]):
    for x in range( 0, bmpSize[0] ):
        
        #Calculate the (x,y) in the plane from the (x,y) in the BMP
        thisX = globalBounds[0] + (float(x)/bmpSize[0])*(globalBounds[2]-globalBounds[0])
        thisY = (float(y)/bmpSize[1])*(globalBounds[3] - globalBounds[1])
        thisY += globalBounds[1]
        
        #Create a complex # representation of this point
        thisPoint = complex(thisX, thisY)

        #Iterate the function f until it grows unbounded
        nxt = f( thisPoint )
        numIters = 0
        while 1:
            dif = nxt-thisPoint
            if abs(nxt - thisPoint) > distanceWhenUnbounded:
                break;
            if numIters >= maxIterationsPerPoint:
                break;
            nxt = f(nxt)
            numIters = numIters+1

        #Convert the number of iterations to a color value
        colorFac = 255.0*float(numIters)/float(maxIterationsPerPoint)
        myRGB = ( colorFac*0.8 + 32, 24+0.1*colorFac, 0.5*colorFac )

        #append this color value to a running list
        myArray.append( int(myRGB[2]) ) #blue first
        myArray.append( int(myRGB[1]) )    #The green
        myArray.append( int(myRGB[0]) )  #Red is last

#Now I reduce the lists to process 0!!
masterString = mpi.reduce( myArray.tostring(), mpi.SUM, 0 )

#Tell user that we're done
message = "process " + str(mpi.rank) + " done with computation!!"
print message

#Process zero does the file writing
if mpi.rank == 0:
    masterArray = array('B')
    masterArray.fromstring(masterString)

    #Write a BMP header
    myBMPHeader = makeBMPHeader( bmpSize[0], bmpSize[1] )
    print "Header length is ", len(myBMPHeader)
    print "BMP size is ", bmpSize
    print "Data length is ", len(masterString)

    #Open the output file and write to the BMP
    outFile = open( 'output.bmp', 'w' )
    outFile.write( myBMPHeader )
    outFile.write( masterString )
    outFile.close()


mpi.barrier()
mpi_end = time.time()
if mpi.rank == 0:
	print '\nPynamic: fractal mpi time = ' + str(mpi_end - mpi_start) + ' secs'
	print 'Pynamic: mpi test passed!\n'
	print 'Pynamic: end time = %10.6f\n' % time.time()
