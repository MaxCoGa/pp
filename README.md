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

- lu = update the local metadata file(pp_pkg_list) with the remote repo list(pkg_list for now)


#### TODO command:
- up FLAG -> upgrade only the packages based on their flag

## TODO
- checksum
- depends
    - check dependencies
    - install dependencies


## paran package example: 
[helloworld](https://github.com/MaxCoGa/helloworld-paran-package)