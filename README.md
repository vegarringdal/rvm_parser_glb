# Rvm Parser Glb

Takes a RVM file and generates 1 merged glb file (per color).

Most if the parsing/triangulation work is taken from this project: https://github.com/cdyk/rvmparser

Needed merging of gltf/reading chucks/parts to keep memory usage low in ACI. Also wanted to learn a little more cpp, so figured  I could try and customize it do what I needed only.

PS! Doing this while I try and learn a little cpp, so it will have some weird parts.

```cli
usage: ./rvm_parser.exe --input INPUT [--output OUTPUT]
[--level LEVEL] [--remove-empty REMOVE-EMPTY] [--cleanup-position
CLEANUP-POSITION] [--cleanup-precision CLEANUP-PRECISION] [--tolerance
TOLERANCE] [--help]

Rvm To Merged GLB (1 mesh per color)

required arguments:
  -i, --input INPUT           rvm fileinput --input ./somefile.rvm

optional arguments:
  -o, --output OUTPUT         Output folder, will create folder if it does not
                              exist. Default is ./exports/
  -l, --level LEVEL           Level to split into files. Default is 0 (site)
  -r, --remove-empty REMOVE-EMPTY
                              Removes elements without primitives. This is
                              enabled by default. To disable use -r 0
  -d, --cleanup-position CLEANUP-POSITION
                              Removes duplicate positions. This is enabled by
                              default. If you want to calculate vertex normals
                              later, then you want this set to 0, to disable use
                              -d 0
  -p, --cleanup-precision CLEANUP-PRECISION
                              Precision to use when cleaning up duplicate
                              positions, default is 3
  -t, --tolerance TOLERANCE   Tolerance to be used in triangulation, default is
                              0.01
  -h, --help                  Display this help message and exit.

```



# Visual Studio Code
* install [vscode 64](https://code.visualstudio.com/)
* install [cmake 64][https://cmake.org/download/]
* install [MinGw-64](https://www.msys2.org/) under `C:\msys64`
  * Follow: https://code.visualstudio.com/docs/cpp/config-mingw
  * in mingGW terminal
    * `pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain`
    * add to path `C:\msys64\ucrt64\bin`
    * Check version
    ```bash
        gcc --version
        g++ --version
        gdb --version
    ```
* install recommended workspace tools
* under cmake extension, select delete cache and reconfugure, and select the gcc

# Visual Studio Community 2022
* install visual studio
* install [cmake 64][https://cmake.org/download/]
* `cmake -S . -B ./build`
* open project file


# Cmake (wsl)
* `apt update`
* `apt upgrade`
* `apt install -y build-essential`
* init
  * `cmake -S . -B ./build`
* for build (wsl/linux)
  * `cmake --build ./build  --config Debug --target all -j 18`
  * `cmake --build ./build  --config Release --target all -j 18`

Linux:
`./build/rvm_parser -i ./samplefilesHuldra/HE-STRU.RVM`

# External libs used
* https://github.com/mmahnic/argumentum/tree/0d9e50d9c8a6e2d829074bdc0ec0fbd932b9f797
* https://github.com/Tencent/rapidjson/tree/ab1842a2dae061284c0a62dca1cc6d5e7e37e346
* https://github.com/memononen/libtess2/tree/fc52516467dfa124bdd967c15c7cf9faf02a34ca
* https://github.com/syoyo/tinygltf/tree/fea67861296293e33e6caee81682261a2700136a
* http://www.zedwood.com/article/cpp-md5-function (md5.cpp/h)
* https://github.com/zeux/meshoptimizer/releases/tag/v0.21 (not in use atm)


# About merged GLB file

All meshes is merged to 1 mesh per color, 1 file per site.


## SomeSiteName.glb

Only 1 scene in each file, and under the scene we have info about draw ranges and id hierarchy.

Draw ranges is useful if you need to set colors/selection using something like threejs batchedmesh, and id hierarchy if you need to show treeview.

```json
{
  "materials": [
    {
      "doubleSided": true,
      "pbrMetallicRoughness": {
        "baseColorFactor": [0.20000001788139343, 0.0, 0.40000003576278687, 1.0],
        "metallicFactor": 0.5,
        "roughnessFactor": 0.5
      }
    }
  ],
  "meshes": [
    {
      "primitives": [
        {
          "attributes": { "POSITION": 1 },
          "indices": 0,
          "material": 0,
          "mode": 4
        }
      ]
    }
  ],
  "nodes": [{ "mesh": 0, "name": "node0" }],
  "scenes": [
    {
      "extras": {
        "draw_ranges_node0": { // node ref to draw_ranges, Record<ID, [START, COUNT]>
          "10": [1620, 36],
          "100": [2772, 36],
          "102": [2808, 36],
          "104": [2844, 36],
          "106": [2880, 36],
          "108": [2916, 36]
        }        },
        "id_hierarchy": { // three, where * is top, Record<ID, [FULLNAME_STRING, ID]>
          "1": ["/HE-INST", "*"],
          "10": ["BOX 1 of EQUIPMENT /27-FV4132", "9"],
          "100": ["BOX 1 of EQUIPMENT /45-PCV4105", "99"],
          "101": ["/45-PCV4107", "2"],
          "102": ["BOX 1 of EQUIPMENT /45-PCV4107", "101"],
          "103": ["/45-PCV4111", "2"],
          "104": ["BOX 1 of EQUIPMENT /45-PCV4111", "103"],
      }
    }
  ]
}

```


## status_file.json

Header info from file, site/root names exported and filename of site/rootname. md5 is from that level in rvm file, not glb file. Can be useful to know if content is changed or not.

```json
{
  "models": [
    {
      "root_name": "/HE-STRU",
      "md5": "22ad7c41355785601c1e300bf4e5edf8",
      "file_name": "$HE-STRU.glb"
    }
  ],
  "warnings": [],
  "header": {
    "date": "Mon Aug 30 17:06:44 2021",
    "encoding": "Unicode UTF-8",
    "info": "AVEVA Everything3D Design Mk2.1.0.25[Z21025-12]  (WINDOWS-NT 6.3)  (25 Feb 2020 : 17:59)",
    "note": "Level 1 to 6",
    "user": "f_pdmsbatch@WS3208",
    "version": 2
  }
}

```