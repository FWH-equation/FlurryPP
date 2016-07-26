TEMPLATE = app
CONFIG += console debug
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD/include \
    lib/tioga/src \
    /usr/lib/openmpi/include

SOURCES += src/global.cpp \
    src/matrix.cpp \
    src/input.cpp \
    src/ele.cpp \
    src/polynomials.cpp \
    src/operators.cpp \
    src/geo.cpp \
    src/output.cpp \
    src/face.cpp \
    src/flux.cpp \
    src/flurry.cpp \
    src/solver.cpp \
    src/intFace.cpp \
    src/boundFace.cpp \
    src/mpiFace.cpp \
    lib/tioga/src/ADT.C \
    lib/tioga/src/MeshBlock.C \
    lib/tioga/src/parallelComm.C \
    lib/tioga/src/tioga.C \
    lib/tioga/src/tiogaInterface.C \
    lib/tioga/src/math.c \
    lib/tioga/src/utils.c \
    src/funcs.cpp \
    src/solver_overset.cpp \
    src/overFace.cpp \
    src/geo_overset.cpp \
    src/points.cpp \
    src/superMesh.cpp \
    src/overComm.cpp \
    src/multigrid.cpp \
    src/flurry_interface.cpp \
    swig/testFlurry.cpp
		   
HEADERS += include/global.hpp \
    include/matrix.hpp \
    include/input.hpp \
    include/ele.hpp \
    include/polynomials.hpp \
    include/operators.hpp \
    include/geo.hpp \
    include/output.hpp \
    include/face.hpp \
    include/flux.hpp \
    include/flurry.hpp \
    include/solver.hpp \
    include/error.hpp \
    include/intFace.hpp \
    include/boundFace.hpp \
    include/mpiFace.hpp \
    lib/tioga/src/ADT.h \
    lib/tioga/src/codetypes.h \
    lib/tioga/src/globals.h \
    lib/tioga/src/MeshBlock.h \
    lib/tioga/src/parallelComm.h \
    lib/tioga/src/tioga.h \
    lib/tioga/src/tiogaInterface.h \
    lib/tioga/src/utils.h \
    include/funcs.hpp \
    include/overFace.hpp \
    include/points.hpp \
    include/superMesh.hpp \
    include/overComm.hpp \
    include/multigrid.hpp \
    include/flurry_interface.hpp \
    swig/flurry.i

DISTFILES += \
    README.md \
    bin/input_3d \
    lib/tioga/src/cellVolume.f90 \
    lib/tioga/src/computeCellVolume.f90 \
    lib/tioga/src/kaiser.f \
    lib/tioga/src/median.F90 \
    lib/tioga/src/makefile

OTHER_FILES += \
    bin/input_supwall \
    tests/euler/cylinder/input_cyl \
    planning \
    makefile
