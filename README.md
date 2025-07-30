# pp
tar -cvf helloworld.tar.gz -C helloworld .

gcc pp.c -o pp

Usage: pp [i|r|s|e] PACKAGENAME | pp [up|lu]


## command:

- i PACKAGENAME = install

- r PACKAGENAME = remove

- s NAME = search

- e PACKAGENAME = search exact name

- u PACKAGENAME = update a package

- up = upgrade all packages that their versions(in pp_info/PACKAGENAME/MANIFEST) are lower than the one in pp_pkg_list

- lu = update the local metadata file(pp_pkg_list) with the remote repo list(pkg_list for now) ul?

- a PACKAGENAME VERSION LOCAL_PATH/URL SHA256 = add a package in pp_pkg_list(local package list)


#### TODO command:
- up FLAG -> upgrade only the packages based on their flag
- c PACKAGENAME -> compile the package if available. should be PACKAGENAME_C in pkg_list. i PACKAGENAME_C will result in the same behavior if choosen

## TODO
- checksum
- depends
    - check dependencies
    - install dependencies
- pass Y flag into u/up down to remove/install
- force update(reinstalling)
- add optionnal install location parameter for install, i PACKAGENAME /PATH/TO/INSTALL/
- keep multiple versions of the same package in pkg_list? so we will be able to chose the version that we want
- package builder (b)

## paran package example: 
[helloworld](https://github.com/MaxCoGa/helloworld-paran-package)