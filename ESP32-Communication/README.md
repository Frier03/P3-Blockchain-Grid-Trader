# ESP32-Communication Development Guide
In here lies the different Espressef IDF project folders for various different programs along with 2 different templates for syntax-highlighting



## Installing and using Espressif IDF on VS Code

**Check out the official docs’ get started section:**
* https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html
* Use version (stable v5.1.1)
* Under Installation, use VS Code as IDE (and install it in VS code)
* Now close the docs website (you wont need to anymore)

**VS Code Espressif IDF installation**
* Once the extension is installed, follow the [Espressif IDF VS Code extension TUTORIAL](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md)
    * Follow very precisely, and install v.5.1.1 when able
    * *(For windows users please install the ESP-IDF Tools installer first and thereafter delete [two folders](https://www.esp32.com/viewtopic.php?t=23976) (idf-git and idf-python folder). Then you can follow the VS code extension tutorial.)*
    * Once Espressif IDF is installed, click on the **basic use** link


**VS Code Espressif IDF Basic use**
> **NOTE**: Til windows skal man IKKE hente DockerWSL eller sige ja tak til Linux subsystem
* Follow the [**basic use**](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/basic_use.md) tutorial and get an example running


**VS Code C++ Configuration**
By default, Espressif IDF uses PATH variables to reach different libraries and so on. Therefore, you have to create the `IDF_PATH` path variable using [this tutorial](https://docs.espressif.com/projects/esp-idf/en/v3.3.5/get-started/add-idf_path-to-profile.html)

> **NOTE**: For Linux, if using `~/.profile` does not work for some reason, put it in `~/.bashrc` instead


<br><br><br>

## Developing on projects in this folder

### When opening a project with VS Code
Make sure to open the folder of the project, rather than the Github folder  
So if you want to open project `station`, right click on the folder `station` and click "Open with Visual Studio Code".  
Alternative: In Visual Code -> File -> Open folder, and then click on the project folder

### When creating a new project
The project folder makes a `.vscode/` folder for you, with relevant configuration for IntelliSense autocomplete
`/station/.vscode/c_cpp_properties.json`. All good!

### When wanting to develop on an already existing project
Because the `.vscode/` is different from computer to computer, it is ignored by in `.gitignore` (to not cause problems).  
Therefore, you have to add it yourself the first time you open an already existing project.  
Do that with the VSCode command: `ESP-IDF: Add vscode configuration folder`!  
This will create the `.vscode/` folder which fits your operating system perfectly!

