# Advanced OpenGL Rendering

## 1. Defered Shading

L'objectif est d'éviter les calculs inutiles (fragments non visibles) en procédant à un rendu en deux étapes.

### 1.1. G-Buffer

Tout d'abord, la méthode `init` de `FBO` est implémentée de manière à stocker les deux textures (couleurs et normales), ainsi que la profondeur.

Ensuite, un objet de type `Shader` a été ajouté pour charger les shaders du G-Buffer. Les shaders ont été configurés pour transmettre la couleur et la normale de chaque fragment.

Un attribut de type `FBO` a été ajouté à la classe `Viewer` et initialisé dans la méthode `init` associée. La méthode `drawDeferred` a également été complétée de manière à activer le FBO, vider ses buffers, activer les shaders du G-Buffer, leur transmettre les variables uniformes nécessaires et "dessiner" les objets de la scène (dans le G-Buffer), avant de désactiver ces shaders ainsi que le `FBO`.

Les images sauvegardées par la méthode `savePNG` témoignent de problèmes dans le code, elles ne correspondent pas à celles attendues.

Couleurs : ![colors-error](images/colors-error.png)

Normales : ![normals-error](images/normals-error.png)

Le problème venait du fait que lors de l'initialisation du FBO, le color attachment 0 était utilisé pour les deux textures (couleurs et normales). En adaptant de manière à automatiser l'indice utilisé, les images furent alors correctes.

Couleurs : ![colors](images/colors.png)
Normales : ![normals](images/normals.png)

### 1.2. Lightning

Un quad était déjà créé pour le sol de la scène dans la méthode `init` de la classe `Viewer`, il faut donc commencer par en créer un autre qui ne sera pas considéré comme un objet de la scène, en le définissant comme attribut de la classe.

Ensuite, un objet de type `Shader` a été ajouté pour charger les shaders Deferred. Les shaders ont été configurés pour récupérer les données du G-Buffer (textures de couleurs et normales comprenant la profondeur et le coefficient spéculaire) et les utiliser pour calculer la position et en déduire l'éclairage à appliquer.

La méthode `drawDeferred` a été complétée de manière à activer les shaders Deferred, leur transmettre les variables uniformes nécessaires et dessiner concrètement le quad, avant de désactiver ces shaders.

Le rendu est censé être le même entre le Forward Shading et le Deferred Shading. Cependant, le calcul de la lumière est **éclaté au sol**, comme le prouve l'image suivante.

![deferred-error-1](images/deferred-error-1.gif)

Le problème venait du fait que la profondeur était mal récupérée. En effet, la méthode `VSPositionFromDepth` du Deferred Fragment Shader nécessite la profondeur, seulement celle utilisée était `gl_FragCoord.z` alors qu'il fallait utiliser celle transmise par le G-Buffer dans `normalTexture.w`. Le problème n'est alors plus le même.

![deferred-error-2](images/deferred-error-2.gif)

Cette fois, l'éclairage est bien plus cohérent masi reste différent de celui du Forward Shading. Une erreur a été commise dans le Deferred Fragment Shader : le vecteur `l` a été normalisé avant sont appel dans la méthode `phong` et son utilisation pour le paramètre `lightCol` n'est alors plus valable. En rétablissant la normalisation uniquement là où nécessaire, on obtient finalement une image équivalente.

![deferred](images/deferred.gif)


<style>
    body {
        width: 600px;
        margin: auto;
    }

    p {
        text-align: justify;
    }

    img {
        display: block;
        border-radius: 12px;
        box-shadow: 12px;
        margin: 12px 0 24px 0;
    }
</style>
