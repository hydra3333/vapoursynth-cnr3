@echo on

REM echo type "D:\TEST\Vapoursynth_x64_R76\test_cnr3_realclip.vpy"
REM type "D:\TEST\Vapoursynth_x64_R76\test_cnr3_realclip.vpy"

set "LOG=test_all_cnr3.log"

echo. >"%LOG%"

echo call "test_cnr3-8bit.bat"
echo. >>"%LOG%"
echo *********** call "test_cnr3-8bit.bat" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3-8bit.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "test_cnr3-8bit_luma_constant_chroma_changes.bat"
echo. >>"%LOG%"
echo *********** call "test_cnr3-8bit_luma_constant_chroma_changes.bat" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3-8bit_luma_constant_chroma_changes.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "test_cnr3-16bit.bat"
echo. >>"%LOG%"
echo *********** call "test_cnr3-16bit.bat" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3-16bit.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "test_cnr3_realclip"
echo. >>"%LOG%"
echo *********** call "test_cnr3_realclip" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3_realclip.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "Test_cnr3_realclip_x10"
echo. >>"%LOG%"
echo *********** call "Test_cnr3_realclip_x10" >>"%LOG%"
echo. >>"%LOG%"
call "Test_cnr3_realclip_x10.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "test_cnr3_scenechange_8bit"
echo. >>"%LOG%"
echo *********** call "test_cnr3_scenechange_8bit" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3_scenechange_8bit.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo call "test_cnr3_scenechange_16bit"
echo. >>"%LOG%"
echo *********** call "test_cnr3_scenechange_16bit" >>"%LOG%"
echo. >>"%LOG%"
call "test_cnr3_scenechange_16bit.bat" 2>>"%LOG%"
echo. >>"%LOG%"

echo.
echo. >>"%LOG%"
echo TYPE "%LOG%"
TYPE "%LOG%"

pause

