# RNorgate

Norgate Investor Services (http://www.premiumdata.net) provides daily 
data on a variety of instruments in files Metastock format.  It's 
not the latest Metastock format and there seem a few wrinkles unique to 
Norgate/PremiumData.

Norgate provides tools to convert these files to csv but this conversion
multiplies the space needed by a large multiple and running the convertor
automatically is flaky. Not all data is exported to the csv files and there 
are no indices to the csv files.

This package allows you to read the binary files in Metastock like format
directly from R. With minor modifications it could also be used to read data
from other vendors who provide or save data in Metastock binary format.

It works for me. No promises on it being stable or anything like that. Use
at your own risk.

I have no affiliation whatsoever to Norgate Investor Services or Metastock.
I'm simply a user of Norgate's services.
