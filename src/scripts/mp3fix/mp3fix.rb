#!/usr/bin/env ruby
#
# Script for fixing VBR encoded mp3 files without XING header. Calculates the real
# track length and adds the XING header to the file.
#
# (c) 2005 Mark Kretschmann <markey@web.de>
# License: GNU General Public License V2


def calcId3v2Size( data )
    # Returns the size of the entire ID3-V2 tag. In other words, the offset from
    # where the real mp3 data starts.
    # @see http://id3lib.sourceforge.net/id3/id3v2com-00.html#sec3.1

    size = data[6]*2**21 + data[7]*2**14 + data[8]*2**7 + data[9]
    size = size + 10 # Header

    return size
end


path = ""
destination = ""


unless $*.empty?()
    path = $*[0]
    destination = path
else
    puts( "Error: Please specify an mp3 file for input.\n" )
    exit()
end

if $*.length() == 2
    destination = $*[1]
end

if not path.include?( ".mp3" ) #FIXME
    puts( "Error: File is not mp3.\n" )
    exit()
end

if not FileTest::exist?( path )
    puts( "Error: File not found.\n" )
    exit()
end


file = File.new( path, "r" )

data = file.read()
id3length = 0
offset = 0

if data[0,3] == "ID3"
    id3length = calcId3v2Size( data )
    puts( "ID3-V2 detected. Tag size: #{offset}\n" )
else
    puts( "ID3-V1 detected.\n" )
end

offset = id3length

SamplesPerFrame = 1152  # Constant for MPEG1 layer 3
BitRateTable = []
BitRateTable << 0 << 32 << 40 << 48 << 56 << 64 << 80 << 96
BitRateTable << 112 << 128 << 160 << 192 << 224 << 256 << 320
SampleRateTable = []
SampleRateTable << 44100 << 48000 << 32100

frameCount = 0
bitCount = 0

# Iterate over all frames
while offset < data.length()
    header = data[offset+0]*2**24 + data[offset+1]*2**16 + data[offset+2]*2**8 + data[offset+3]

    bitrate = BitRateTable[( header & 0x0000f000 ) >> 12] * 1000
    samplerate = SampleRateTable[( header & 0x00000c00 ) >> 10]
    padding = ( header & 0x00000200 ) >> 9

    frameSize = ( SamplesPerFrame / 8 * bitrate ) / samplerate + padding

#     puts( "bitrate     : #{bitrate.to_s()}\n" )
#     puts( "samplerate  : #{samplerate.to_s()}\n" )
#     puts( "padding     : #{padding.to_s()}\n" )
#     puts( "framesize   : #{frameSize}\n" )
#     puts( "\n" )

    frameCount += 1
    bitCount += bitrate

    offset += frameSize
end


averageBitrate = bitCount / frameCount
length = data.length() / averageBitrate * 8

puts( "Number of frames : #{frameCount}\n" )
puts( "Average bitrate  : #{averageBitrate}\n" )
puts( "Length (seconds) : #{length}\n" )


xing = String.new()
xing << "Xing"

flags = 0x0002  # Frames and Bytes fields valid
xing << 0 << 0 << 0 << flags

xing << ( ( frameCount & 0xff000000 ) >> 24 )
xing << ( ( frameCount & 0x00ff0000 ) >> 16 )
xing << ( ( frameCount & 0x0000ff00 ) >> 8 )
xing << ( ( frameCount & 0x000000ff ) >> 0 )

xing << ( ( data.length() & 0xff000000 ) >> 24 )
xing << ( ( data.length() & 0x00ff0000 ) >> 16 )
xing << ( ( data.length() & 0x0000ff00 ) >> 8 )
xing << ( ( data.length() & 0x000000ff ) >> 0 )


# Insert XING header into string, after the first MPEG header
data[id3length + 4, 0] = xing


destfile = File::open( destination, File::CREAT|File::TRUNC|File::WRONLY )
destfile << data

puts( "done.\n" )

