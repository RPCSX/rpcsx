<div align="center">
   
# RPCSX
*Un émulateur expérimental pour PS4 (PlayStation 4) pour Linux écrit en C++*

[Français](readme-fr.md) | [Anglais](readme.md)

[![Compiler RPCSX](../../../actions/workflows/rpcsx.yml/badge.svg)](../../../actions/workflows/rpcsx.yml)

[![Vérification du Formatting](../../../actions/workflows/format.yml/badge.svg)](../../../actions/workflows/format.yml)

[![](https://img.shields.io/discord/252023769500090368?color=5865F2&logo=discord&logoColor=white)](https://discord.gg/t6dzA4wUdG)

</div>

> **Attention** <br/>
> Il n'est pas encore possible d'exécuter des jeux, et il n'y a pas de date précise pour que cela change.

> Ne demandez pas de lien vers des jeux ou des fichiers système. Le piratage n'est pas autorisé sur le Github ni dans la Discord.

## Contribution

Si vous souhaitez contribuer en tant que développeur, veuillez nous contacter sur [Discord](https://discord.gg/t6dzA4wUdG) (Anglais).

## Building

Tout d'abord, installez les dependencies pour les distributions Debian-like.
   
``sudo apt install build-essential cmake libunwind-dev libglfw3-dev libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools libspirv-cross-c-shared-dev libxbyak-dev``

## Cloner le repo

``git clone https://github.com/RPCSX/rpcsx && cd rpcsx``
   
## Compiler l'émulateur
   
`mkdir -p build && cd build && cmake .. && cmake --build .`

## Comment créer un disque dur (HHD) virtuel

> La PS4 a un système de fichiers est sensible aux majuscules et minuscules. Pour créer le disque dur virtuel, procédez comme suit :
 
`truncate -s 512M ps4-hdd.exfat`

`mkfs.exfat -n PS4-HDD ./ps4-hdd.exfat`

`mkdir ps4-fs`

``sudo mount -t exfat -o uid=`id -u`,gid=`id -g` ./ps4-hdd.exfat ./ps4-fs``

## Comment exécuter des programmes de test et des jeux
   
Utliser le message de sortie de `rpcsx-os` (`-h` argument), ou rejoignez le [Discord](https://discord.gg/t6dzA4wUdG) pour de l'aide

Vous pouvez exécuter l'émulateur avec quelques programmes de teste en utilisant cette commande :
   
`rm -f /dev/shm/rpcsx-* && ./rpcsx-os --mount  "<path to fw>/system" "/system" --mount "<path to 'game' root>" /app0 /app0/some-test-sample.elf [<args for test elf>...]`

## Création d'un journal (Log)

Vous pouvez utiliser cette argument si vous avez rencontré une segfault à des fins de débogage.
    
`--trace` 
    
Vous pouvez rediriger tous les messages de journal vers un fichier en ajoutant cette commande :

`&>log.txt`
      


## Licence

RPCSX est sous la licence GPLV2, à l'exception des répertoires contenant leur propre fichier de licence ou des fichiers contenant leur propre licence.
Ainsi, Orbis-Kernel est sous la licence du MIT.
