.. _bluetooth_mesh_pca20036:

PCA20036
#########

This sample implements the PCA20036 ethernet functionality. 
DFU over TFTP (using MCUboot as bootloader) and an ethernet command system is implemented.

Windows 10
***********

IMPORTANT:
The ethernet adapter must be set to private. This can be done using this command:
(Run the command from *Administrator PowerShell*)

Set-NetConnectionProfile -InterfaceAlias "Ethernet" -NetworkCategory Private

Change "Ethernet" to the name of your ethernet adapter (the name can be found by running 'ipconfig').


To start a DFU
===============

Build the sample, launch misc/tftp_server/tftpd64.exe and execute misc/backend/dfu_all.py.

The pca20036 sample firmware will now be transferred over TFTP and loaded into the boards.
