# RNorgate

I haven't used this for a long time. It probably has some serious bugs
and other problems with it. I've had reports of it not compiling on
Windows. There be dragons here. I not longer use it. It's probably
not too hard to make it work. If you find this repository useful, please
fork it.


Norgate Investor Services (http://www.premiumdata.net) provides daily 
data on a variety of instruments in files Metastock format.

Norgate provides tools to convert these files to csv but this conversion
multiplies the space needed by a large multiple and there is no good reason
to convert numerical data to text only to have to parse the text when you
have to read it.

This package allows you to read the binary files in Metastock format
directly from R. It could likely be used to  be used to read data
from other vendors who provide or save data in Metastock format but I haven't
tried it.

It works for me. No promises on it being stable or anything like that. Use
at your own risk.

I have no affiliation whatsoever to Norgate Investor Services or Metastock.
I'm simply a very happy user of Norgate's services.
