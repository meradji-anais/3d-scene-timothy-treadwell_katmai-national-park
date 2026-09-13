#version 120

// Inputs depuis vertex shader
varying vec3 vNormal;
varying vec3 vPosition;
varying vec2 vTexCoord;
varying vec4 vColor;  // recevoir la couleur du vertex

// Uniforms
uniform int uMode;          // 1=Lambertien, 2=Speculaire, 3=Glossy
uniform int uLightType;     // 0=Directionnelle, 1=Ponctuelle, 2=Spot
uniform int uUseTexture;    // 1=utiliser texture, 0=utiliser couleur

uniform vec3 uLightDir;
uniform vec3 uLightPos;
uniform vec3 uSpotDir;
uniform float uSpotInnerCut;
uniform float uSpotOuterCut;
uniform vec3 uViewPos;

uniform sampler2D texture0;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L;
    float attenuation = 1.0;

    //  CALCUL DIRECTION LUMIÈRE 

    if (uLightType == 0) {
        // Directionnelle
        L = normalize(uLightDir);
    } else if (uLightType == 1) {
        // Ponctuelle
        vec3 lightVec = uLightPos - vPosition;
        float distance = length(lightVec);
        L = normalize(lightVec);
        attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    } else {
        // Spot
        vec3 lightVec = uLightPos - vPosition;
        float distance = length(lightVec);
        L = normalize(lightVec);
        
        attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        
        vec3 spotDirection = normalize(uSpotDir);
        float theta = dot(L, -spotDirection);
        float epsilon = uSpotInnerCut - uSpotOuterCut;
        float intensity = clamp((theta - uSpotOuterCut) / epsilon, 0.0, 1.0);
        attenuation *= intensity;
    }

    // COMPOSANTE AMBIANTE 
    vec3 ambient = vec3(0.3, 0.3, 0.35);  

    // COMPOSANTE DIFFUSE (Lambertien) 

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);  

    // COMPOSANTE SPÉCULAIRE 

    vec3 specular = vec3(0.0);

    if (uMode == 2 || uMode == 3) {
        vec3 V = normalize(uViewPos - vPosition);
        vec3 R = reflect(-L, N);

        float shininess = (uMode == 2) ? 128.0 : 32.0;
        float spec = pow(max(dot(R, V), 0.0), shininess);
        specular = spec * vec3(0.5, 0.5, 0.5);
    }

    //  COMBINAISON FINALE 
    vec3 lighting = ambient + (diffuse + specular) * attenuation;
    
    // Choisir entre texture et couleur du matériau

    vec4 texColor = texture2D(texture0, vTexCoord);
    vec3 baseColor;
    
    // Si la texture a de l'alpha > 0.1, on l'utilise, sinon couleur du vertex

    if (uUseTexture == 1 && texColor.a > 0.1) {
        baseColor = texColor.rgb;
    } else {
        baseColor = vColor.rgb;
    }
    
    vec3 result = lighting * baseColor;

    gl_FragColor = vec4(result, 1.0);
}