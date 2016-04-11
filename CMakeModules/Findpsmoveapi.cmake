FIND_PATH(PSMOVEAPI_INCLUDE_DIR psmoveapi/psmove.h
  PATHS
  $ENV{PSMOVEAPI_HOME}
  NO_DEFAULT_PATH
    PATH_SUFFIXES include
)

FIND_PATH(PSMOVEAPI_INCLUDE_DIR psmoveapi/psmove.h
  PATHS
  /usr/local/include
  /usr/include
  /sw/include # Fink
  /opt/local/include # DarwinPorts
  /opt/csw/include # Blastwave
  /opt/include
)

FIND_LIBRARY(PSMOVEAPI_LIBRARY 
  NAMES psmoveapi
  PATHS $ENV{PSMOVEAPI_HOME}
    NO_DEFAULT_PATH
    PATH_SUFFIXES lib64 lib
)
FIND_LIBRARY(PSMOVEAPI_LIBRARY 
  NAMES psmoveapi
  PATHS
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt
    /usr/freeware
  PATH_SUFFIXES lib64 lib
)

FIND_LIBRARY(PSMOVETRACKER_LIBRARY 
  NAMES psmoveapi_tracker
  PATHS $ENV{PSMOVEAPI_HOME}
    NO_DEFAULT_PATH
    PATH_SUFFIXES lib64 lib
)
FIND_LIBRARY(PSMOVETRACKER_LIBRARY 
  NAMES psmoveapi_tracker
  PATHS
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt
    /usr/freeware
  PATH_SUFFIXES lib64 lib
)

SET(PSMOVEAPI_FOUND "NO")
IF(PSMOVEAPI_LIBRARY AND PSMOVETRACKER_LIBRARY AND PSMOVEAPI_INCLUDE_DIR)
  SET(PSMOVEAPI_FOUND "YES")
ENDIF(PSMOVEAPI_LIBRARY AND PSMOVETRACKER_LIBRARY AND PSMOVEAPI_INCLUDE_DIR)

