import os
import sys
import xml.etree.ElementTree as ET

a = ET.Element("a")
b0 = ET.SubElement(a, "b0")
b1 = ET.SubElement(a, "b1")
s = ET.dump(a)
print s
