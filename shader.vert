#version 120

varying vec3 vNormal;
varying vec3 vPosition;
varying vec2 vTexCoord;
varying vec4 vColor;  //  transmettre la couleur au fragment shader

void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vPosition = (gl_ModelViewMatrix * gl_Vertex).xyz;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    vTexCoord = gl_MultiTexCoord0.xy;
    vColor = gl_Color;  //  transmettre la couleur du vertex
}