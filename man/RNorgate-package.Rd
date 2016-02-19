\name{RNorgate-package}
\alias{RNorgate-package}
\alias{RNorgate}
\docType{package}
\title{
Reads data provided by Norgate Investor Services.
}
\description{
    Reads data provided by Norgate Investor Services http://www.premiumdata.net
    Can likely be used to read data from any source in Metastock format.
}
\details{
\tabular{ll}{
Package: \tab RNorgate\cr
Type: \tab Package\cr
Version: \tab 1.0\cr
Date: \tab 2013-02-01\cr
License: \tab GPL\cr
}
Pretty straightforward package. Reads data. That's it.
}
\author{
Ashwin Kapur <ashwin.kapur@gmail.com>

Maintainer: Ashwin Kapur <ashwin.kapur@gmail.com>
}
\references{
premiumdata.net, The website for Norgate Investor Services.
}
\keyword{ package }
\seealso{
}
\examples{
library(RNorgate)

data.rep = createNorgate("PATH_TO_METASTOCK_DIR")

# Next line provides list of assets for which data can be read from data files
# in the directory
assets(data.rep)

## Gets data and puts it into a data frame
data.for.asset = data.rep["TICKER"]
}
