#!/usr/bin/python3
from enum import Enum
import sys
import yaml

# Resource ids
def RsrcIdToInt(s):
  # Global
  if s == "sRsrcType":
    return 1
  elif s == "sRsrcName":
    return 2
  elif s == "MinorBaseOS":
    return 10
  elif s == "MinorLength":
    return 11
  elif s == "BoardId":
    return 32
  elif s == "VendorInfo":
    return 36
  # VendorInfo
  elif s == "VendorID":
    return 1
  elif s == "SerialNum":
    return 2
  elif s == "RevLevel":
    return 3
  elif s == "PartNum":
    return 4
  elif s == "Date":
    return 5
  else:
    raise ValueError("Unknown resource type: " + str(s))

def addWord(data, value):
  # Add word to data bytearray
  data.append((value >> 8) & 0xff)
  data.append(value & 0xff)

def addLong(data, value):
  # Add long to data bytearray
  data.append((value >> 24) & 0xff)
  data.append((value >> 16) & 0xff)
  data.append((value >> 8) & 0xff)
  data.append(value & 0xff)


class VidParamsData:
  def __init__(self, data):
    self._data = data

  def generate(self):
    # Encode VidParams (with thanks to Basilisk II) and align to 4 bytes
    data = bytearray()

    addLong(data, 50)      # Size
    addLong(data, 0)       # Base offset

    # bytes per row
    addWord(data, self._data["bytesPerRow"])

    # Bounds
    addWord(data, 0)
    addWord(data, 0)

    # hres/vres
    addWord(data, self._data["hres"])
    addWord(data, self._data["vres"])    

    addWord(data, 0)       # Version
    addWord(data, 0)       # Pack type
    addLong(data, 0)       # Pack size
    addLong(data, 0x480000)  # hres
    addLong(data, 0x480000)  # vres

    # Pixel type and size (in bits)
    addWord(data, self._data["pixelType"])
    addWord(data, self._data["pixelSize"])

    # CmpCount and CmpSize (?)
    addWord(data, self._data["cmpCount"])
    addWord(data, self._data["cmpSize"])

    addLong(data, 0)       # Plane size
    addLong(data, 0)       # Reserved
    
    while len(data) % 4:
      data.extend("\0".encode("ascii"))

    return (data, len(data))


class ResourceListData:
  def __init__(self, data):
    self._data = data
    
  def generate(self):
    # Encode resource list
    res_list = ResourceList(self._data)
    res_list.parse()

    # Pass through the already generated result
    gen = res_list.generate()
    data = gen[3]

    return (data, len(data))


class InlineWordData:
  def __init__(self, data):
    self._data = data
    
  def generate(self):
    # Encode InlineWord as 2 bytes
    data = bytearray()
    addWord(data, self._data)

    return (data, len(data))


class RsrcTypeData:
  def __init__(self, data):
    self._data = data
    
  def generate(self):
    # Encode RsrcType as 8 bytes
    data = bytearray()
    addWord(data, self._data["category"])
    addWord(data, self._data["cType"])
    addWord(data, self._data["drSW"])
    addWord(data, self._data["drHw"])

    return (data, len(data))


class LongResourceData:
  def __init__(self, data):
    self._data = data
    
  def generate(self):
    # Encode Long as big-endian
    data = bytearray()
    addLong(data, self._data)

    return (data, len(data))


class StringResourceData:
  def __init__(self, data):
    self._data = data;

  def generate(self):
    # Encode String with terminating 0 and aligned to 4 bytes
    data = bytearray()
    data.extend(self._data.encode("ascii"))

    if len(data) % 4 == 0:
      data.extend("\0\0\0\0".encode("ascii"))

    while len(data) % 4:
      data.extend("\0".encode("ascii"))

    return (data, len(data))


class Resource:
  def __init__(self, _resource):
    self._resource = _resource

  def parse(self):
    if self._resource["type"] == "Long":
      self.data = LongResourceData(self._resource["data"])
    elif self._resource["type"] == "String":
      self.data = StringResourceData(self._resource["data"])
    elif self._resource["type"] == "RsrcType":
      self.data = RsrcTypeData(self._resource["data"])
    elif self._resource["type"] == "InlineWord":
      self.data = InlineWordData(self._resource["data"])
    elif self._resource["type"] == "VidParams":
      self.data = VidParamsData(self._resource["data"])
    elif self._resource["type"] == "ResourceList":
      self.data = ResourceListData(self._resource)
    else:
      raise ValueError("Unknown type " + self._resource["type"])

  def generate(self):
    # Return (id, type, len, data) tuple
    datagen = self.data.generate()

    # If id is a string, try and use our internal id list. Otherwise
    # use the number direct.
    if type(self._resource["id"]) is str:
      rsrc_id = RsrcIdToInt(self._resource["id"])
    else:
      rsrc_id = int(self._resource["id"])

    return (rsrc_id, self._resource["type"], datagen[1], datagen[0])


class ResourceList:

  def __init__(self, _resourcelist):
    self._resourcelist = _resourcelist
    self.resources = []

  def parse(self): 
    _resources = self._resourcelist["resources"]
    for _resource in _resources:
      res = Resource(_resource)
      res.parse()
      self.resources.append(res)

  def generate(self):
    # Return (id, list offset, len, data) tuple
    gens = []
    for resource in self.resources:
      # Generate the data, keeping track of total size
      gen = resource.generate()
      gens.append(gen)

    data = bytearray()
    for gen in gens:
      # Combine all the data into single tuple
      if gen[1] != "InlineWord":
        data.extend(gen[3])

    # Now generate each record calculating soffset
    soffset = -len(data)
    roffset = len(data)
    for gen in gens:
      rsrc = bytearray()
      rsrc.append(gen[0])

      if gen[1] == "InlineWord":
        rsrc.append(0)
        rsrc.extend(gen[3])
        data.extend(rsrc)
      else:
        rsrc.extend(soffset.to_bytes(3, "big", signed=True))
        data.extend(rsrc)
        # -4 adjusts for the space taken by the entry itself
        soffset = soffset + gen[2] - 4

    # Append end of list marker
    rsrc = bytearray()
    rsrc.append(0xff)
    rsrc.append(0)
    rsrc.append(0)
    rsrc.append(0)
    data.extend(rsrc)

    return (self._resourcelist["id"], roffset, len(data), data)


class Directory:

  def __init__(self, _directory):
    self._directory = _directory
    self.resourcelists = []

  def parse(self):
    #print(self._directory)

    if self._directory:    
      for _directory_res in self._directory:
        res_list = ResourceList(_directory_res)
        res_list.parse()
        self.resourcelists.append(res_list)

  def generate(self):
    # Return (list offset, len, data) tuple
    gens = []
    for resourcelist in self.resourcelists:
      gen = resourcelist.generate()
      gens.append(gen)
      
    data = bytearray()
    for gen in gens:
      # Combine all the data into single tuple
      data.extend(gen[3])

    # Now generate each record calculating soffset
    soffset = -len(data)
    roffset = len(data)
    for gen in gens:        
      rsrc = bytearray()
      rsrc.append(gen[0])
      # Resource records live at roffset (gen[1])
      rsrc.extend((soffset + gen[1]).to_bytes(3, "big", signed=True))

      data.extend(rsrc)
      # -4 adjusts for the space taken by the entry itself
      soffset = soffset + gen[2] - 4

    # Append end of list marker
    rsrc = bytearray()
    rsrc.append(0xff)
    rsrc.append(0)
    rsrc.append(0)
    rsrc.append(0)
    data.extend(rsrc)

    return (roffset, len(data), data)


def rom_crc(data):
  # Calculate the ROM crc for data (with FBlock included)
  crc = 0
  for i in data:
    crc = (crc << 1) | (crc >> 31)
    crc = (crc + i) & 0xffffffff

  return crc


def writeFBlock(gen):
  # Append the FBlock (20 bytes) to the end of the generated output
  data = gen[2]

  # Directory offset
  offset = gen[0] - gen[1]
  data.append(0xff)
  data.extend(offset.to_bytes(3, "big", signed=True))

  # Length of declaration data
  addLong(data, gen[1] + 20)
  #addLong(data, 0)

  # CRC (to be done later)
  addLong(data, 0)

  # Revision level/format
  addWord(data, 0x0101)

  # Test pattern
  addLong(data, 0x5a932bc7)
    
  # Byte lanes
  addWord(data, 0x000f)

  # Calculate CRC and insert it
  crc = rom_crc(data)
  crc_offset = len(data) - 12
  data[crc_offset] = (crc >> 24) & 0xff
  data[crc_offset + 1] = (crc >> 16) & 0xff
  data[crc_offset + 2] = (crc >> 8) & 0xff
  data[crc_offset + 3] = crc & 0xff

  return data


# Open the declaration ROM yaml
filename = sys.argv[1]
yamlfile = open(filename, 'r')
directory = yaml.load(yamlfile)

#print(yaml.dump(directory))

parser = Directory(directory)
parser.parse()
gen = parser.generate()

#print(gen)

data = writeFBlock(gen)

romfile = open("decl.rom", 'wb')
romfile.write(data)
romfile.close()
