if exist ..\Headquarter (
  rmdir /S /Q Dependencies\Headquarter
  mklink /J Dependencies\Headquarter ..\Headquarter
)