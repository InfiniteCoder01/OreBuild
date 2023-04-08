# OreBuild
 Library for json: https://github.com/nlohmann/json

 Usage:
 ```
 OreBuild (build/run/search/install) [githubPackageID]
 ```

 To build for linux, install g++ and git and run the following command:
 ```
 git clone https://github.com/InfiniteCoder01/OreBuild.git && g++ -std=c++17 -Ofast src/*.cpp -o bin/OreBuild && sudo cp ./bin/OreBuild /usr/bin
 ```
 Or you can create a link to it:
 ```
 sudo ln -sf ./bin/OreBuild /usr/bin/OreBuild
 ```
