CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Nemosys)

MESSAGE(STATUS "ACCESSING TEST DIRECTORY: ${PROJECT_SOURCE_DIR}")

# macro to list subdirectories of a given current
MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

SUBDIRLIST(TEST_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)

SET(TEST_DATA ${CMAKE_CURRENT_LIST_DIR}/test_data)
SET(CUBATURE_TESTDIR ${TEST_DATA}/CubatureTest)
SET(CONVERSION_TESTDIR ${TEST_DATA}/ConversionTest)
SET(ORTHOPOLY_TESTDIR ${TEST_DATA}/OrthoPolyTest)
SET(PATCHRECOVERY_TESTDIR ${TEST_DATA}/PatchRecoveryTest)
SET(TRANSFER_TESTDIR ${TEST_DATA}/TransferTest)
SET(MESHGEN_TESTDIR ${TEST_DATA}/MeshGenTest)
SET(PNTGEN_TESTDIR ${TEST_DATA}/PNTGenTest)
SET(AUTOVERIF_TESTDIR ${TEST_DATA}/AutoVerificationTest)
SET(meshgendefault_in ${MESHGEN_TESTDIR}/default_in.json)
SET(meshgendefault ${MESHGEN_TESTDIR}/default.json)
SET(meshgenunif_in ${MESHGEN_TESTDIR}/unif_in.json)
SET(meshgenunif ${MESHGEN_TESTDIR}/unif.json)
SET(meshgengeom_in ${MESHGEN_TESTDIR}/geom_in.json)
SET(meshgengeom ${MESHGEN_TESTDIR}/geom.json)
SET(pntgenbench1_in ${PNTGEN_TESTDIR}/bench1_in.json)
SET(pntgenbench1 ${PNTGEN_TESTDIR}/bench1.json)
SET(pntgenbench5_in ${PNTGEN_TESTDIR}/bench5_in.json)
SET(pntgenbench5 ${PNTGEN_TESTDIR}/bench5.json)
SET(pntgenbench6_in ${PNTGEN_TESTDIR}/bench6_in.json)
SET(pntgenbench6 ${PNTGEN_TESTDIR}/bench6.json)
CONFIGURE_FILE( ${meshgendefault_in} ${meshgendefault} @ONLY )
CONFIGURE_FILE( ${meshgenunif_in} ${meshgenunif} @ONLY )
CONFIGURE_FILE( ${meshgengeom_in} ${meshgengeom} @ONLY )
CONFIGURE_FILE( ${pntgenbench1_in} ${pntgenbench1} @ONLY )
CONFIGURE_FILE( ${pntgenbench5_in} ${pntgenbench5} @ONLY )
CONFIGURE_FILE( ${pntgenbench6_in} ${pntgenbench6} @ONLY )


ADD_TEST(NAME cubatureInterpTest COMMAND runCubatureInterpTest ${CUBATURE_TESTDIR}/cube_refined.vtu ${CUBATURE_TESTDIR}/cube_refinedGaussPoints.vtp ${CUBATURE_TESTDIR}/cube_refinedGaussPointsNoData.vtp ${CUBATURE_TESTDIR}/integrationREF.vtu ${CUBATURE_TESTDIR}/single-hex.vtu)

ADD_TEST(NAME conversionTest COMMAND runConversionTest ${CONVERSION_TESTDIR}/case0001_ref.vtu ${CONVERSION_TESTDIR}/case0001.msh ${CONVERSION_TESTDIR}/hinge.vtu ${CONVERSION_TESTDIR}/hinge.vol ${CONVERSION_TESTDIR}/legacyVTK1.vtk ${CONVERSION_TESTDIR}/legacyVTK2.vtk ${CONVERSION_TESTDIR}/legacyVTK1_ref.vtu ${CONVERSION_TESTDIR}/legacyVTK2_ref.vtu ${CONVERSION_TESTDIR}/gorilla.vtp ${CONVERSION_TESTDIR}/pnt-tri.pntmesh ${CONVERSION_TESTDIR}/pnt-tri-ref.vtu
    ${CONVERSION_TESTDIR}/pnt-quad.pntmesh ${CONVERSION_TESTDIR}/pnt-quad-ref.vtu ${CONVERSION_TESTDIR}/pnt-hex.pntmesh ${CONVERSION_TESTDIR}/pnt-hex-ref.vtu ${CONVERSION_TESTDIR}/pnt-mix.pntmesh ${CONVERSION_TESTDIR}/pnt-mix-ref.vtu)

ADD_TEST(NAME orthoPolyTest COMMAND runOrthoPolyTest ${ORTHOPOLY_TESTDIR}/F.txt)
ADD_TEST(NAME patchRecoveryTest COMMAND runPatchRecoveryTest ${PATCHRECOVERY_TESTDIR}/case0001.vtu ${PATCHRECOVERY_TESTDIR}/testRef.vtu ${PATCHRECOVERY_TESTDIR}/fixedWithData.vtu)
ADD_TEST(NAME transferTest COMMAND runTransferTest ${TRANSFER_TESTDIR}/pointSource.vtu ${TRANSFER_TESTDIR}/cellSource.vtu ${TRANSFER_TESTDIR}/target.vtu ${TRANSFER_TESTDIR}/pntRef.vtu ${TRANSFER_TESTDIR}/cellRef.vtu)
ADD_TEST(NAME meshGenTest COMMAND runMeshGenTest ${MESHGEN_TESTDIR}/default.json ${MESHGEN_TESTDIR}/hingeRef.vtu ${MESHGEN_TESTDIR}/unif.json ${MESHGEN_TESTDIR}/hingeUnifRef.vtu ${MESHGEN_TESTDIR}/geom.json ${MESHGEN_TESTDIR}/hingeGeomRef.vtu)
ADD_TEST(NAME autVerifTest COMMAND runAutoVerifTest ${AUTOVERIF_TESTDIR}/finer.vtu ${AUTOVERIF_TESTDIR}/fine.vtu ${AUTOVERIF_TESTDIR}/coarse.vtu ${AUTOVERIF_TESTDIR}/richardson.vtu)

ADD_TEST(NAME PNTGenTest COMMAND runPNTGenTest
    ${PNTGEN_TESTDIR}/bench1.json ${PNTGEN_TESTDIR}/bench1_conv_gold.pntmesh
    ${PNTGEN_TESTDIR}/bench5.json ${PNTGEN_TESTDIR}/bench5_conv_gold.pntmesh
    ${PNTGEN_TESTDIR}/bench6.json ${PNTGEN_TESTDIR}/bench6_conv_gold.pntmesh)
#Python wrapper testing

FIND_PACKAGE(PythonInterp 2.7 REQUIRED)
IF (ENABLE_PYTHON_BINDINGS)
    if (NOT PYTHONINTERP_FOUND)
      message(STATUS "Python interpreter NOT found")
    else(NOT PYTHONINTERP_FOUND)
      message(STATUS "Python interpreter found")
      ADD_TEST(NAME testPyNemosys
               COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_scripts/test_pyNemosys27.py
               --topsrcdir ${CMAKE_CURRENT_SOURCE_DIR} )
           set_property(TEST testPyNemosys
                        PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib/:${CMAKE_INSTALL_PREFIX}/vtk/lib")
           set_property(TEST testPyNemosys
                        APPEND
                        PROPERTY ENVIRONMENT "PYTHONPATH=${CMAKE_INSTALL_PREFIX}/Nemosys/python/src:${CMAKE_INSTALL_PREFIX}/Nemosys/python/lib/python2.7/site-packages")
    endif (NOT PYTHONINTERP_FOUND)
ENDIF()
