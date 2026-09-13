#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#define MAX_PARTICLES 300

// STRUCTURES

typedef struct {
    float x, y, z;
    float speedY;
    float speedX;
    bool active;
} Particle;

typedef struct {
    char name[128];
    float ambient[3];
    float diffuse[3];
    float specular[3];
    float shininess;
    char textureFile[256];
    GLuint textureID;
} Material;

typedef struct {
    float* vertices;
    float* normals;
    float* texcoords;
    int* faces;
    int* materialIndices;
    int vertexCount;
    int normalCount;
    int texcoordCount;
    int faceCount;
    Material* materials;
    int materialCount;
} OBJModel;

// VARIABLES GLOBALES

GLuint texBear, texFox, texGrass, texMountain, texSky, texTree, texTronc, texRock, texRiver, texSalmon, texEagle;

Particle particles[MAX_PARTICLES];

OBJModel bear1Model = {0};
OBJModel bear2Model = {0};
OBJModel foxModel = {0};
OBJModel timothyModel = {0};
OBJModel salmonModel = {0};
OBJModel eagleModel = {0};
OBJModel riverModel = {0};

float timeElapsed = 0.0f;
float waterOffset = 0.0f;
float dayNightCycle = 0.0f;

float foxX = -5.0f;
float foxZ = -4.0f;
float foxAngle = 90.0f;
float foxSpeed = 0.01f;
int foxDirection = 1;
float foxLegAngle = 0.0f;
bool foxAutoMove = true;
bool foxMovedManually = false;
bool foxMovedVertically = false;

float cameraDistance = 10.0f;
float cameraHeight = 2.5f;
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;
float targetCameraDistance = 10.0f;
float zoomSpeed = 1.0f;
int lastMouseX = 0;
int lastMouseY = 0;
bool mouseRotating = false;

bool light1On = true;
bool light2On = false;
bool autoDayNight = true;

const float INITIAL_CAMERA_DISTANCE = 10.0f;
const float INITIAL_CAMERA_HEIGHT = 2.5f;
const float INITIAL_CAMERA_ANGLE_X = 0.0f;
const float INITIAL_CAMERA_ANGLE_Y = 0.0f;

// VARIABLES SHADERS

GLuint shaderProgram = 0;
int illuminationMode = 3;
int lightType = 0;
bool useShadersForScene = true;

// FONCTIONS SHADERS

char* loadShaderSource(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("ERREUR: Impossible d'ouvrir le shader %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = (char*)malloc(length + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }

    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    return source;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("ERREUR COMPILATION SHADER:\n%s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createShaderProgram(const char* vertPath, const char* fragPath) {
    char* vertSource = loadShaderSource(vertPath);
    char* fragSource = loadShaderSource(fragPath);

    if (!vertSource || !fragSource) {
        printf("ERREUR: Impossible de charger les shaders\n");
        if (vertSource) free(vertSource);
        if (fragSource) free(fragSource);
        return 0;
    }

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSource);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSource);

    free(vertSource);
    free(fragSource);

    if (!vertShader || !fragShader) {
        if (vertShader) glDeleteShader(vertShader);
        if (fragShader) glDeleteShader(fragShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("ERREUR LINKAGE PROGRAMME:\n%s\n", infoLog);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

// CHARGEMENT TEXTURES

GLuint loadTexture(const char* filename) {
    GLuint textureID = 0;
    int width, height, nrChannels;

    FILE* testFile = fopen(filename, "rb");
    if (!testFile) return 0;
    fclose(testFile);

    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (!data) return 0;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return textureID;
}

// CHARGEMENT MTL

void loadMTL(const char* path, OBJModel* model) {
    FILE* file = fopen(path, "r");
    if (!file) return;

    char line[512];
    int maxMaterials = 50;
    model->materials = (Material*)malloc(sizeof(Material) * maxMaterials);
    model->materialCount = 0;

    Material* currentMat = NULL;
    char basePath[256];
    strcpy(basePath, path);
    char* lastSlash = strrchr(basePath, '\\');
    if (!lastSlash) lastSlash = strrchr(basePath, '/');
    if (lastSlash) *(lastSlash + 1) = '\0';
    else basePath[0] = '\0';

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "newmtl ", 7) == 0) {
            currentMat = &model->materials[model->materialCount++];
            sscanf(line, "newmtl %127s", currentMat->name);
            currentMat->ambient[0] = currentMat->ambient[1] = currentMat->ambient[2] = 0.2f;
            currentMat->diffuse[0] = currentMat->diffuse[1] = currentMat->diffuse[2] = 0.8f;
            currentMat->specular[0] = currentMat->specular[1] = currentMat->specular[2] = 0.0f;
            currentMat->shininess = 0.0f;
            currentMat->textureFile[0] = '\0';
            currentMat->textureID = 0;
        }
        else if (currentMat && strncmp(line, "Ka ", 3) == 0) {
            sscanf(line, "Ka %f %f %f", &currentMat->ambient[0], &currentMat->ambient[1], &currentMat->ambient[2]);
        }
        else if (currentMat && strncmp(line, "Kd ", 3) == 0) {
            sscanf(line, "Kd %f %f %f", &currentMat->diffuse[0], &currentMat->diffuse[1], &currentMat->diffuse[2]);
        }
        else if (currentMat && strncmp(line, "Ks ", 3) == 0) {
            sscanf(line, "Ks %f %f %f", &currentMat->specular[0], &currentMat->specular[1], &currentMat->specular[2]);
        }
        else if (currentMat && strncmp(line, "Ns ", 3) == 0) {
            sscanf(line, "Ns %f", &currentMat->shininess);
        }
        else if (currentMat && strncmp(line, "map_Kd ", 7) == 0) {
            char texName[256];
            sscanf(line, "map_Kd %255s", texName);
            char fullPath[512];
            sprintf(fullPath, "%s%s", basePath, texName);
            strcpy(currentMat->textureFile, fullPath);
            currentMat->textureID = loadTexture(fullPath);
        }
    }

    fclose(file);
}


// CHARGEMENT OBJ

void loadOBJ(const char* path, OBJModel* model, bool loadMaterials) {
    FILE* file = fopen(path, "r");
    if (!file) return;

    char line[512];
    int maxVertices = 200000;
    int maxNormals = 200000;
    int maxTexcoords = 200000;
    int maxFaces = 300000;

    model->vertices = (float*)malloc(sizeof(float) * 3 * maxVertices);
    model->normals = (float*)malloc(sizeof(float) * 3 * maxNormals);
    model->texcoords = (float*)malloc(sizeof(float) * 2 * maxTexcoords);
    model->faces = (int*)malloc(sizeof(int) * 9 * maxFaces);
    model->materialIndices = (int*)malloc(sizeof(int) * maxFaces);

    model->vertexCount = 0;
    model->normalCount = 0;
    model->texcoordCount = 0;
    model->faceCount = 0;
    model->materialCount = 0;

    int currentMaterialIndex = -1;
    char mtlFile[256] = "";

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "mtllib ", 7) == 0 && loadMaterials && mtlFile[0] == '\0') {
            char* mtlStart = line + 7;
            while (*mtlStart == ' ' || *mtlStart == '\t') mtlStart++;
            int i = 0;
            while (mtlStart[i] != '\0' && mtlStart[i] != '\n' && mtlStart[i] != '\r' && i < 255) {
                mtlFile[i] = mtlStart[i];
                i++;
            }
            mtlFile[i] = '\0';

            char basePath[256];
            strcpy(basePath, path);
            char* lastSlash = strrchr(basePath, '\\');
            if (!lastSlash) lastSlash = strrchr(basePath, '/');
            if (lastSlash) *(lastSlash + 1) = '\0';
            else basePath[0] = '\0';
            char fullMtlPath[512];
            sprintf(fullMtlPath, "%s%s", basePath, mtlFile);
            loadMTL(fullMtlPath, model);
        }
        else if (strncmp(line, "usemtl ", 7) == 0 && loadMaterials) {
            char matName[128];
            sscanf(line, "usemtl %127s", matName);
            for (int i = 0; i < model->materialCount; i++) {
                if (strcmp(model->materials[i].name, matName) == 0) {
                    currentMaterialIndex = i;
                    break;
                }
            }
        }
        else if (strncmp(line, "v ", 2) == 0) {
            float x, y, z;
            if (sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
                model->vertices[model->vertexCount * 3 + 0] = x;
                model->vertices[model->vertexCount * 3 + 1] = y;
                model->vertices[model->vertexCount * 3 + 2] = z;
                model->vertexCount++;
            }
        }
        else if (strncmp(line, "vt ", 3) == 0) {
            float u, v;
            if (sscanf(line, "vt %f %f", &u, &v) == 2) {
                model->texcoords[model->texcoordCount * 2 + 0] = u;
                model->texcoords[model->texcoordCount * 2 + 1] = v;
                model->texcoordCount++;
            }
        }
        else if (strncmp(line, "vn ", 3) == 0) {
            float nx, ny, nz;
            if (sscanf(line, "vn %f %f %f", &nx, &ny, &nz) == 3) {
                model->normals[model->normalCount * 3 + 0] = nx;
                model->normals[model->normalCount * 3 + 1] = ny;
                model->normals[model->normalCount * 3 + 2] = nz;
                model->normalCount++;
            }
        }
        else if (strncmp(line, "f ", 2) == 0) {
            int v[3] = {0,0,0}, vt[3] = {0,0,0}, n[3] = {0,0,0};

            int parsed = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                   &v[0], &vt[0], &n[0], &v[1], &vt[1], &n[1], &v[2], &vt[2], &n[2]);

            if (parsed != 9) {
                parsed = sscanf(line, "f %d//%d %d//%d %d//%d",
                       &v[0], &n[0], &v[1], &n[1], &v[2], &n[2]);
                vt[0] = vt[1] = vt[2] = 0;
            }

            if (parsed == 9 || parsed == 6) {
                bool valid = true;
                for (int i = 0; i < 3; i++) {
                    if (v[i] < 1 || v[i] > model->vertexCount) valid = false;
                    if (n[i] < 1 || n[i] > model->normalCount) valid = false;
                    if (vt[i] > 0 && vt[i] > model->texcoordCount) valid = false;
                }
                if (!valid) continue;

                for (int i = 0; i < 3; i++) {
                    model->faces[model->faceCount * 9 + i*3 + 0] = v[i] - 1;
                    model->faces[model->faceCount * 9 + i*3 + 1] = vt[i] - 1;
                    model->faces[model->faceCount * 9 + i*3 + 2] = n[i] - 1;
                }
                model->materialIndices[model->faceCount] = currentMaterialIndex;
                model->faceCount++;
            }
        }
    }

    fclose(file);
}

// SYSTÈME DE PARTICULES

void initParticles() {
    srand(time(NULL));
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x = (float)(rand() % 100 - 50);
        particles[i].y = (float)(rand() % 30 + 5);
        particles[i].z = (float)(rand() % 40 - 20);
        particles[i].speedY = -0.02f - (float)(rand() % 10) / 1000.0f;
        particles[i].speedX = (float)(rand() % 10 - 5) / 1000.0f;
        particles[i].active = true;
    }
}

void updateParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].y += particles[i].speedY;
            particles[i].x += particles[i].speedX;

            if (particles[i].y < 0.0f) {
                particles[i].y = 25.0f + (float)(rand() % 10);
                particles[i].x = (float)(rand() % 100 - 50);
                particles[i].z = (float)(rand() % 40 - 20);
            }
        }
    }
}

void drawParticles() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
    glPointSize(3.0f);

    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            glVertex3f(particles[i].x, particles[i].y, particles[i].z);
        }
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// FONCTIONS DE RENDU PRIMITIVES

void drawGround() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGrass);
    glColor3f(1,1,1);

    float radius = 80.0f;
    int divisions = 8;
    float step = radius * 2.0f / divisions;

    for(int i = 0; i < divisions; i++) {
        for(int j = 0; j < divisions; j++) {
            float x1 = -radius + i * step;
            float x2 = x1 + step;
            float z1 = -radius + j * step;
            float z2 = z1 + step;

            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glTexCoord2f(0, 0); glVertex3f(x1, 0, z1);
            glTexCoord2f(8, 0); glVertex3f(x2, 0, z1);
            glTexCoord2f(8, 8); glVertex3f(x2, 0, z2);
            glTexCoord2f(0, 8); glVertex3f(x1, 0, z2);
            glEnd();
        }
    }

    glDisable(GL_TEXTURE_2D);
}

void drawSky() {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texSky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    float brightness = 0.5f + 0.5f * cos(dayNightCycle);
    glColor3f(brightness, brightness, brightness * 1.1f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);

    float s = 100.0f;
    int slices = 64;
    int stacks = 32;

    for (int i = 0; i < stacks; i++) {
        float lat0 = M_PI * (-0.5f + (float)i / stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);
        float z0 = sin(lat0);
        float z1 = sin(lat1);
        float r0 = cos(lat0);
        float r1 = cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2 * M_PI * (float)j / slices;
            float x = cos(lng);
            float y = sin(lng);
            float u = (float)j / (float)slices;
            if (j == slices) u = 1.01f;
            float v0 = (float)i / (float)stacks;
            float v1 = (float)(i + 1) / (float)stacks;

            glTexCoord2f(u, v1);
            glVertex3f(s * x * r1, s * z1, s * y * r1);
            glTexCoord2f(u, v0);
            glVertex3f(s * x * r0, s * z0, s * y * r0);
        }
        glEnd();
    }

    glPopMatrix();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPopAttrib();
    glEnable(GL_DEPTH_TEST);
}

void drawTree(float x, float z, float height) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texTronc);
    glColor3f(1.0f, 1.0f, 1.0f);

    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(-90, 1, 0, 0);
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluCylinder(quad, 0.15f, 0.1f, height * 0.6f, 12, 4);
    gluDeleteQuadric(quad);
    glPopMatrix();

    glBindTexture(GL_TEXTURE_2D, texTree);
    glColor3f(0.9f, 1.0f, 0.9f);

    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glTranslatef(x, height * 0.3f + i * height * 0.2f, z);
        glRotatef(-90, 1, 0, 0);
        quad = gluNewQuadric();
        gluQuadricTexture(quad, GL_TRUE);
        gluQuadricNormals(quad, GLU_SMOOTH);
        gluCylinder(quad, 0.5f - i * 0.1f, 0.0f, height * 0.4f, 16, 4);
        gluDeleteQuadric(quad);
        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);
}

void drawRock(float x, float y, float z, float size) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texRock);
    glColor3f(1.0f, 1.0f, 1.0f);

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(x * 45.0f, 0, 1, 0);
    glRotatef(z * 30.0f, 1, 0, 0);
    glScalef(size, size * 0.6f, size * 0.8f);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, 0.3f, 16, 12);
    gluDeleteQuadric(quad);

    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}

void drawMountainRange(float startX, float z, float baseWidth, float height) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texMountain);
    glColor3f(1,1,1);

    int segments = 25;
    for (int i=0; i<segments; i++){
        float x1 = startX + i*baseWidth/segments;
        float x2 = x1 + baseWidth/segments;
        float xMid = (x1+x2)/2;
        float position = (float)i/segments;
        float distanceFromPeak = fabs(position - 0.35f);
        float peakBoost = (distanceFromPeak<0.15f)?(1.0f-distanceFromPeak/0.15f)*0.8f:0.0f;
        float baseHeight = 0.55f + 0.15f*sin(i*0.6f);
        float peakHeight = height*(baseHeight+peakBoost);
        float spread = 8.0f;
        float distanceFactor = (z+22.0f)/10.0f;
        float baseOffset = -0.6f*distanceFactor;

        glBegin(GL_TRIANGLES);
        glNormal3f(0,1,0);
        glTexCoord2f(0,0); glVertex3f(x1-spread, baseOffset, z);
        glTexCoord2f(1,0); glVertex3f(x2+spread, baseOffset, z);
        glTexCoord2f(0.5,1); glVertex3f(xMid, peakHeight, z-0.4f);
        glEnd();
    }
}

void drawAllMountains() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texMountain);

    drawMountainRange(-35.0f, -20.0f, 70.0f, 10.5f);
    drawMountainRange(-30.0f, -17.0f, 60.0f, 9.2f);
    drawMountainRange(-25.0f, -14.0f, 50.0f, 4.0f);

    glPushMatrix();
    glTranslatef(-20.0f, 0, 0);
    glRotatef(90, 0, 1, 0);
    drawMountainRange(-30.0f, -20.0f, 60.0f, 9.0f);
    drawMountainRange(-25.0f, -17.0f, 50.0f, 8.5f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(20.0f, 0, 0);
    glRotatef(-90, 0, 1, 0);
    drawMountainRange(-30.0f, -20.0f, 60.0f, 9.0f);
    drawMountainRange(-25.0f, -17.0f, 50.0f, 7.5f);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
}

// RENDU MODÈLES OBJ AVEC MATÉRIAUX

void drawModelWithMaterials(OBJModel* model, float x, float y, float z, float scale, float rotY, float rotX) {
    if (model->vertexCount == 0 || model->faceCount == 0) return;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glScalef(scale, scale, scale);

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);

    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    bool usingShaders = (currentProgram != 0);

    if (!usingShaders) {
        glDisable(GL_COLOR_MATERIAL);
    }

    int currentMat = -1;
    GLint locUseTexture = -1;

    if (usingShaders) {
        locUseTexture = glGetUniformLocation(currentProgram, "uUseTexture");
    }

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < model->faceCount; i++) {
        int matIdx = model->materialIndices[i];

        if (matIdx != currentMat && matIdx >= 0 && matIdx < model->materialCount) {
            glEnd();

            Material* mat = &model->materials[matIdx];

            if (!usingShaders) {
                GLfloat ambient[] = {mat->ambient[0], mat->ambient[1], mat->ambient[2], 1.0f};
                GLfloat diffuse[] = {mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0f};
                GLfloat specular[] = {mat->specular[0], mat->specular[1], mat->specular[2], 1.0f};

                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->shininess);
            }

            if (mat->textureID > 0) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, mat->textureID);
                glColor3f(1.0f, 1.0f, 1.0f);

                if (usingShaders && locUseTexture != -1) {
                    glUniform1i(locUseTexture, 1);
                }
            } else {
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glColor3f(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2]);

                if (usingShaders && locUseTexture != -1) {
                    glUniform1i(locUseTexture, 0);
                }
            }

            currentMat = matIdx;
            glBegin(GL_TRIANGLES);
        }

        for (int j = 0; j < 3; j++) {
            int vi = model->faces[i * 9 + j * 3 + 0];
            int vti = model->faces[i * 9 + j * 3 + 1];
            int ni = model->faces[i * 9 + j * 3 + 2];

            if (ni >= 0 && ni < model->normalCount) {
                glNormal3f(model->normals[ni * 3 + 0], model->normals[ni * 3 + 1], model->normals[ni * 3 + 2]);
            }

            if (vti >= 0 && vti < model->texcoordCount) {
                glTexCoord2f(model->texcoords[vti * 2 + 0], model->texcoords[vti * 2 + 1]);
            }

            if (vi >= 0 && vi < model->vertexCount) {
                glVertex3f(model->vertices[vi * 3 + 0], model->vertices[vi * 3 + 1], model->vertices[vi * 3 + 2]);
            }
        }
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_NORMALIZE);

    if (!usingShaders) {
        glEnable(GL_COLOR_MATERIAL);
    }

    glPopMatrix();
}

void drawAnimatedFox(OBJModel* model, GLuint texture, float x, float y, float z, float scale, float rotY, float legAngle, bool useMaterial) {
    if (model->vertexCount == 0 || model->faceCount == 0) return;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);

    float bobbing = sin(legAngle * 2.0f) * 0.015f;
    glTranslatef(0.0f, bobbing, 0.0f);

    glScalef(scale, scale, scale);

    glPushAttrib(GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT);

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);

    if (useMaterial && model->materialCount > 0) {
        glDisable(GL_COLOR_MATERIAL);
        int currentMat = -1;

        glBegin(GL_TRIANGLES);
        for (int i = 0; i < model->faceCount; i++) {
            int matIdx = model->materialIndices[i];

            if (matIdx != currentMat && matIdx >= 0 && matIdx < model->materialCount) {
                glEnd();

                Material* mat = &model->materials[matIdx];

                GLfloat ambient[] = {mat->ambient[0], mat->ambient[1], mat->ambient[2], 1.0f};
                GLfloat diffuse[] = {mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0f};
                GLfloat specular[] = {mat->specular[0], mat->specular[1], mat->specular[2], 1.0f};

                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->shininess);

                if (mat->textureID > 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, mat->textureID);
                    glColor3f(1.0f, 1.0f, 1.0f);
                } else {
                    glDisable(GL_TEXTURE_2D);
                    glColor3f(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2]);
                }

                currentMat = matIdx;
                glBegin(GL_TRIANGLES);
            }

            for (int j = 0; j < 3; j++) {
                int vi = model->faces[i * 9 + j * 3 + 0];
                int vti = model->faces[i * 9 + j * 3 + 1];
                int ni = model->faces[i * 9 + j * 3 + 2];

                if (ni >= 0 && ni < model->normalCount) {
                    glNormal3f(model->normals[ni*3], model->normals[ni*3+1], model->normals[ni*3+2]);
                }
                if (vti >= 0 && vti < model->texcoordCount) {
                    glTexCoord2f(model->texcoords[vti*2], model->texcoords[vti*2+1]);
                }
                if (vi >= 0 && vi < model->vertexCount) {
                    glVertex3f(model->vertices[vi*3], model->vertices[vi*3+1], model->vertices[vi*3+2]);
                }
            }
        }
        glEnd();
    } else {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_TRIANGLES);
        for (int i = 0; i < model->faceCount; i++) {
            for (int j = 0; j < 3; j++) {
                int vi = model->faces[i * 9 + j * 3 + 0];
                int vti = model->faces[i * 9 + j * 3 + 1];
                int ni = model->faces[i * 9 + j * 3 + 2];

                if (ni >= 0 && ni < model->normalCount) {
                    glNormal3f(model->normals[ni*3], model->normals[ni*3+1], model->normals[ni*3+2]);
                }

                if (vti >= 0 && vti < model->texcoordCount) {
                    glTexCoord2f(model->texcoords[vti*2], model->texcoords[vti*2+1]);
                } else if (vi >= 0 && vi < model->vertexCount) {
                    float vx = model->vertices[vi*3];
                    float vz = model->vertices[vi*3+2];
                    glTexCoord2f(vx*0.5f+0.5f, vz*0.5f+0.5f);
                }

                if (vi >= 0 && vi < model->vertexCount) {
                    glVertex3f(model->vertices[vi*3], model->vertices[vi*3+1], model->vertices[vi*3+2]);
                }
            }
        }
        glEnd();
    }

    glPopAttrib();
    glPopMatrix();
}

void drawFish(OBJModel* model, GLuint texture, float x, float z, float scale, float rotY, float swimOffset) {
    if (model->vertexCount == 0 || model->faceCount == 0) return;

    float waveHeight = 0.015f;
    float waveY = 0.01f + sin(x * 5.0f + waterOffset + swimOffset) * waveHeight;

    glPushMatrix();
    glTranslatef(x, waveY, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glScalef(scale, scale, scale);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < model->faceCount; i++) {
        for (int j = 0; j < 3; j++) {
            int vi  = model->faces[i * 9 + j * 3 + 0];
            int vti = model->faces[i * 9 + j * 3 + 1];
            int ni  = model->faces[i * 9 + j * 3 + 2];

            if (ni >= 0 && ni < model->normalCount) {
                glNormal3f(model->normals[ni*3], model->normals[ni*3+1], model->normals[ni*3+2]);
            }
            if (vti >= 0 && vti < model->texcoordCount) {
                glTexCoord2f(model->texcoords[vti*2], model->texcoords[vti*2+1]);
            }
            if (vi >= 0 && vi < model->vertexCount) {
                glVertex3f(model->vertices[vi*3], model->vertices[vi*3+1], model->vertices[vi*3+2]);
            }
        }
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_NORMALIZE);
    glEnable(GL_CULL_FACE);
    glPopMatrix();
}

void drawAnimatedRiver(OBJModel* model, float x, float z, float scaleX, float scaleY, float scaleZ) {
    if (model->vertexCount == 0 || model->faceCount == 0) return;

    glPushMatrix();
    glTranslatef(x, 0.008f, z);
    glScalef(scaleX, scaleY, scaleZ);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);

    if (model->materialCount > 0) {
        glDisable(GL_COLOR_MATERIAL);

        int currentMat = -1;

        glBegin(GL_TRIANGLES);
        for (int i = 0; i < model->faceCount; i++) {
            int matIdx = model->materialIndices[i];

            if (matIdx != currentMat && matIdx >= 0 && matIdx < model->materialCount) {
                glEnd();

                Material* mat = &model->materials[matIdx];

                GLfloat ambient[] = {mat->ambient[0], mat->ambient[1], mat->ambient[2], 1.0f};
                GLfloat diffuse[] = {mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 0.8f};
                GLfloat specular[] = {mat->specular[0], mat->specular[1], mat->specular[2], 1.0f};

                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->shininess);

                if (mat->textureID > 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, mat->textureID);
                } else {
                    glDisable(GL_TEXTURE_2D);
                }

                currentMat = matIdx;
                glBegin(GL_TRIANGLES);
            }

            for (int j = 0; j < 3; j++) {
                int vi = model->faces[i * 9 + j * 3 + 0];
                int vti = model->faces[i * 9 + j * 3 + 1];
                int ni = model->faces[i * 9 + j * 3 + 2];

                if (ni >= 0 && ni < model->normalCount) {
                    glNormal3f(model->normals[ni*3], model->normals[ni*3+1], model->normals[ni*3+2]);
                }

                if (vti >= 0 && vti < model->texcoordCount) {
                    float u = model->texcoords[vti*2] + waterOffset * 0.05f;
                    float v = model->texcoords[vti*2+1];
                    glTexCoord2f(u, v);
                }

                if (vi >= 0 && vi < model->vertexCount) {
                    float vx = model->vertices[vi*3];
                    float vy = model->vertices[vi*3+1];
                    float vz = model->vertices[vi*3+2];

                    float wave = sin(waterOffset * 2.0f + vx * 3.0f + vz * 2.0f) * 0.015f;

                    glVertex3f(vx, vy + wave, vz);
                }
            }
        }
        glEnd();

        glEnable(GL_COLOR_MATERIAL);
    } else {
        glBindTexture(GL_TEXTURE_2D, texRiver);
        glColor4f(0.4f, 0.6f, 0.9f, 0.8f);

        glBegin(GL_TRIANGLES);
        for (int i = 0; i < model->faceCount; i++) {
            for (int j = 0; j < 3; j++) {
                int vi = model->faces[i * 9 + j * 3 + 0];
                int vti = model->faces[i * 9 + j * 3 + 1];
                int ni = model->faces[i * 9 + j * 3 + 2];

                if (ni >= 0 && ni < model->normalCount) {
                    glNormal3f(model->normals[ni*3], model->normals[ni*3+1], model->normals[ni*3+2]);
                }

                if (vti >= 0 && vti < model->texcoordCount) {
                    float u = model->texcoords[vti*2] + waterOffset * 0.05f;
                    float v = model->texcoords[vti*2+1];
                    glTexCoord2f(u, v);
                } else if (vi >= 0 && vi < model->vertexCount) {
                    float u = model->vertices[vi*3] * 0.2f;
                    float v = model->vertices[vi*3+2] * 0.2f;
                    glTexCoord2f(u + waterOffset * 0.05f, v);
                }

                if (vi >= 0 && vi < model->vertexCount) {
                    float vx = model->vertices[vi*3];
                    float vy = model->vertices[vi*3+1];
                    float vz = model->vertices[vi*3+2];

                    float wave = sin(waterOffset * 2.0f + vx * 3.0f + vz * 2.0f) * 0.015f;
                    glVertex3f(vx, vy + wave, vz);
                }
            }
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_NORMALIZE);
    glPopMatrix();
}

// INITIALISATION

void init() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        printf("ERREUR GLEW: %s\n", glewGetErrorString(err));
        exit(1);
    }

    shaderProgram = createShaderProgram("shader.vert", "shader.frag");
    if (!shaderProgram) {
        printf("ATTENTION: Shaders non disponibles\n");
        useShadersForScene = false;
    }

    glClearColor(0.3f, 0.35f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.3f, 0.35f, 0.4f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, 60.0f);

    glEnable(GL_LIGHT0);
    GLfloat light0_pos[] = {-5.0f, 8.0f, 5.0f, 1.0f};
    GLfloat light0_ambient[] = {0.2f, 0.2f, 0.25f, 1.0f};
    GLfloat light0_diffuse[] = {0.5f, 0.5f, 0.55f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);

    GLfloat light1_pos[] = {3.0f, 5.0f, 3.0f, 1.0f};
    GLfloat light1_ambient[] = {0.15f, 0.12f, 0.08f, 1.0f};
    GLfloat light1_diffuse[] = {0.4f, 0.35f, 0.25f, 1.0f};
    GLfloat light1_specular[] = {0.2f, 0.2f, 0.15f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);

    const char* basePath = "C:\\Users\\meradji\\Documents\\Master2\\InformatiqueGraphique\\TPs\\scene_katmai_glut\\";

    char path[512];
    sprintf(path, "%sbear.jpg", basePath); texBear = loadTexture(path);
    sprintf(path, "%sfox.jpg", basePath); texFox = loadTexture(path);
    sprintf(path, "%sgrass.jpg", basePath); texGrass = loadTexture(path);
    sprintf(path, "%smountain.jpg", basePath); texMountain = loadTexture(path);
    sprintf(path, "%ssky.jpg", basePath); texSky = loadTexture(path);
    sprintf(path, "%stree.jpg", basePath); texTree = loadTexture(path);
    sprintf(path, "%stronc.jpg", basePath); texTronc = loadTexture(path);
    sprintf(path, "%srock.jpg", basePath); texRock = loadTexture(path);
    sprintf(path, "%sriver.jpg", basePath); texRiver = loadTexture(path);
    sprintf(path, "%sMr._Sockeye_baseColor.png", basePath); texSalmon = loadTexture(path);

    sprintf(path, "%sbear1.obj", basePath); loadOBJ(path, &bear1Model, true);
    sprintf(path, "%sbear2.obj", basePath); loadOBJ(path, &bear2Model, true);
    sprintf(path, "%sfox.obj", basePath); loadOBJ(path, &foxModel, true);
    sprintf(path, "%stimothyyy.obj", basePath); loadOBJ(path, &timothyModel, true);
    sprintf(path, "%ssaumon.obj", basePath); loadOBJ(path, &salmonModel, false);
    sprintf(path, "%sAmerican_Bald_Eagle.obj", basePath); loadOBJ(path, &eagleModel, true);
    sprintf(path, "%sriver.obj", basePath); loadOBJ(path, &riverModel, true);

    if (timothyModel.materialCount == 0) {
        sprintf(path, "%stimothyyy.mtl", basePath);
        loadMTL(path, &timothyModel);
    }

    initParticles();

    printf("CONTROLES:\n");
    printf("1-2-3 : Modes illumination (Lambertien/Speculaire/Glossy)\n");
    printf("4-5-6 : Types lumiere (Directionnelle/Ponctuelle/Spot)\n");
    printf("S     : Activer/Desactiver shaders\n");
    printf("ESPACE: Pause/Play fox\n");
    printf("Fleches: Deplacer fox\n");
    printf("Z/D   : Zoom\n");
    printf("R     : Reset camera\n");
    printf("0     : Lumiere secondaire\n");
    printf("N     : Cycle jour/nuit\n");
    printf("ESC   : Quitter\n");
}

// AFFICHAGE PRINCIPAL

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float brightness = 0.5f + 0.5f * cos(dayNightCycle);
    GLfloat globalAmbient[] = {brightness * 0.3f, brightness * 0.3f, brightness * 0.35f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, -cameraHeight, 0.0f);

    if (light2On) glEnable(GL_LIGHT1);
    else glDisable(GL_LIGHT1);

    // RENDU SANS SHADERS (pipeline fixe)
    glUseProgram(0);

    drawSky();
    drawAllMountains();
    drawGround();

    glPushMatrix();
    glTranslatef(-5.0f, 0.0f, 0.0f);
    glRotatef(5.0f, 0.0f, 1.6f, 0.0f);
    drawAnimatedRiver(&riverModel, 0.0f, 0.0f, 3.8f, 0.00003f, 0.08f);
    glPopMatrix();

    drawRock(-8.5f, 0.0f, -1.2f, 0.7f);
    drawRock(-7.2f, 0.0f, -1.15f, 0.65f);
    drawRock(-5.5f, 0.0f, -7.5f, 0.8f);
    drawRock(-3.0f, 0.0f, -9.0f, 0.9f);
    drawRock(-4.5f, 0.0f, -6.5f, 0.7f);
    drawRock(-2.5f, 0.0f, -8.5f, 0.85f);
    drawRock(-14.0f, 0.0f, 0.0f, 1.1f);
    drawRock(13.5f, 0.0f, -1.0f, 1.0f);

    drawRock(-4.6f, 0.0f, -1.2f, 0.7f);
    drawRock(-3.3f, 0.0f, -1.18f, 0.68f);
    drawRock(-2.0f, 0.0f, -1.22f, 0.72f);
    drawRock(-0.7f, 0.0f, -1.2f, 0.7f);
    drawRock(0.6f, 0.0f, -1.25f, 0.75f);
    drawRock(1.9f, 0.0f, -1.15f, 0.65f);
    drawRock(3.2f, 0.0f, -1.2f, 0.7f);
    drawRock(4.5f, 0.0f, -1.18f, 0.68f);
    drawRock(5.8f, 0.0f, -1.22f, 0.73f);
    drawRock(-8.5f, 0.0f, 1.2f, 0.7f);
    drawRock(-7.2f, 0.0f, 1.15f, 0.65f);
    drawRock(-5.9f, 0.0f, 1.25f, 0.75f);
    drawRock(-4.6f, 0.0f, 1.2f, 0.7f);
    drawRock(-3.3f, 0.0f, 1.18f, 0.68f);
    drawRock(-2.0f, 0.0f, 1.22f, 0.72f);
    drawRock(3.2f, 0.0f, 1.2f, 0.7f);
    drawRock(4.5f, 0.0f, 1.18f, 0.68f);
    drawRock(5.8f, 0.0f, 1.22f, 0.73f);
    drawRock(-3.5f, 0.0f, -2.0f, 1.0f);
    drawRock(1.2f, 0.0f, -3.5f, 0.8f);
    drawRock(-7.5f, 0.0f, -1.5f, 1.2f);
    drawRock(8.5f, 0.0f, -2.5f, 0.9f);
    drawRock(-2.0f, 0.0f, -4.5f, 1.1f);
    drawRock(5.5f, 0.0f, -3.0f, 0.7f);
    drawRock(-9.0f, 0.0f, -7.0f, 1.3f);
    drawRock(3.8f, 0.0f, -6.5f, 0.85f);
    drawRock(-5.5f, 0.0f, -5.0f, 0.95f);
    drawRock(7.2f, 0.0f, -1.5f, 1.05f);

    drawFish(&salmonModel, texSalmon, -7.8f, 0.1f, 0.32f, 1.3f, timeElapsed);
    drawFish(&salmonModel, texSalmon, -6.5f, -0.2f, 0.30f, 0.3f, timeElapsed + 1.0f);
    drawFish(&salmonModel, texSalmon, -5.0f, 0.3f, 0.33f, 1.0f, timeElapsed + 2.0f);
    drawFish(&salmonModel, texSalmon, -3.8f, -0.15f, 0.31f, 0.5f, timeElapsed + 0.5f);
    drawFish(&salmonModel, texSalmon, -2.5f, 0.15f, 0.29f, 4.0f, timeElapsed + 1.5f);
    drawFish(&salmonModel, texSalmon, 7.8f, 0.1f, 0.32f, 1.3f, timeElapsed);
    drawFish(&salmonModel, texSalmon, 6.5f, -0.2f, 0.30f, 0.3f, timeElapsed + 1.0f);
    drawFish(&salmonModel, texSalmon, 5.0f, 0.3f, 0.33f, 1.0f, timeElapsed + 2.0f);
    drawFish(&salmonModel, texSalmon, 3.8f, -0.15f, 0.31f, 0.5f, timeElapsed + 0.5f);
    drawFish(&salmonModel, texSalmon, 2.5f, 0.15f, 0.29f, 4.0f, timeElapsed + 1.5f);

    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_FOG);
    drawModelWithMaterials(&eagleModel, -10.0f, 7.0f, -8.0f, 0.015f, 45.0f, 0.0f);
    drawModelWithMaterials(&eagleModel, 6.0f, 6.5f, -10.0f, 0.012f, -30.0f, 0.0f);
    drawModelWithMaterials(&eagleModel, 1.0f, 8.0f, -12.0f, 0.010f, 90.0f, 0.0f);
    glPopAttrib();

    drawTree(-7.0f, -8.0f, 2.8f);
    drawTree(-4.5f, -10.0f, 3.0f);
    drawTree(-9.5f, -5.0f, 2.6f);
    drawTree(6.5f, -9.0f, 2.9f);
    drawTree(8.5f, -6.5f, 2.7f);
    drawTree(4.0f, -11.0f, 3.1f);
    drawTree(-2.0f, 7.5f, 2.5f);
    drawTree(3.5f, 8.0f, 2.8f);
    drawTree(-5.5f, 8.5f, 2.6f);
    drawTree(7.0f, 7.0f, 2.9f);
    drawTree(-12.0f, 2.0f, 2.7f);
    drawTree(11.0f, 1.0f, 2.8f);
    drawTree(-13.0f, -2.0f, 2.9f);
    drawTree(12.5f, -3.0f, 2.6f);

    // RENDU AVEC SHADERS (personnages principaux)
    if (useShadersForScene && shaderProgram) {
        glUseProgram(shaderProgram);

        GLint locMode = glGetUniformLocation(shaderProgram, "uMode");
        GLint locLightType = glGetUniformLocation(shaderProgram, "uLightType");
        GLint locLightDir = glGetUniformLocation(shaderProgram, "uLightDir");
        GLint locLightPos = glGetUniformLocation(shaderProgram, "uLightPos");
        GLint locSpotDir = glGetUniformLocation(shaderProgram, "uSpotDir");
        GLint locSpotInner = glGetUniformLocation(shaderProgram, "uSpotInnerCut");
        GLint locSpotOuter = glGetUniformLocation(shaderProgram, "uSpotOuterCut");
        GLint locViewPos = glGetUniformLocation(shaderProgram, "uViewPos");

        if (locMode != -1) glUniform1i(locMode, illuminationMode);
        if (locLightType != -1) glUniform1i(locLightType, lightType);
        if (locViewPos != -1) glUniform3f(locViewPos, 0.0f, cameraHeight, cameraDistance);

        if (lightType == 0) {
            if (locLightDir != -1) glUniform3f(locLightDir, -0.5f, -1.0f, -0.3f);
        } else if (lightType == 1) {
            if (locLightPos != -1) glUniform3f(locLightPos, 2.0f, 5.0f, 2.0f);
        } else if (lightType == 2) {
            if (locLightPos != -1) glUniform3f(locLightPos, 2.0f, 8.0f, 2.0f);
            if (locSpotDir != -1) glUniform3f(locSpotDir, -0.5f, -1.0f, -0.5f);
            if (locSpotInner != -1) glUniform1f(locSpotInner, 0.94f);
            if (locSpotOuter != -1) glUniform1f(locSpotOuter, 0.86f);
        }

        drawModelWithMaterials(&timothyModel, 1.6f, 0.0f, 4.0f, 0.07f, 20.0f, 0.0f);
        drawModelWithMaterials(&bear1Model, -1.0f, 0.7f, -8.0f, 2.5f, 30, 0.0f);
        drawModelWithMaterials(&bear2Model, 20.0f, 0.0f, 22.0f, 1.2f, 30.0f, 0.0f);
        drawAnimatedFox(&foxModel, texFox, foxX, 0.12f, foxZ, 9.0f, foxAngle, foxLegAngle, true);
        drawAnimatedFox(&foxModel, texFox, 0.3f, 0.03f, 4.0f, 12.0f, 30.0f, 0.0f, true);

        glUseProgram(0);
    } else {
        drawModelWithMaterials(&timothyModel, 1.6f, 0.0f, 4.0f, 0.07f, 20.0f, 0.0f);
        drawModelWithMaterials(&bear1Model, -1.0f, 0.7f, -8.0f, 2.5f, 30, 0.0f);
        drawModelWithMaterials(&bear2Model, 20.0f, 0.0f, 22.0f, 1.2f, 30.0f, 0.0f);
        drawAnimatedFox(&foxModel, texFox, foxX, 0.12f, foxZ, 9.0f, foxAngle, foxLegAngle, true);
        drawAnimatedFox(&foxModel, texFox, 0.3f, 0.03f, 4.0f, 12.0f, 30.0f, 0.0f, true);
    }

    drawParticles();

    glutSwapBuffers();
}

// GESTION DES ÉVÉNEMENTS

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50.0, (float)w / (float)h, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case '1':
            illuminationMode = 1;
            printf("Mode: LAMBERTIEN (diffus)\n");
            break;
        case '2':
            illuminationMode = 2;
            printf("Mode: SPECULAIRE (miroir)\n");
            break;
        case '3':
            illuminationMode = 3;
            printf("Mode: GLOSSY (semi-brillant)\n");
            break;
        case '4':
            lightType = 0;
            printf("Lumiere: DIRECTIONNELLE\n");
            break;
        case '5':
            lightType = 1;
            printf("Lumiere: PONCTUELLE\n");
            break;
        case '6':
            lightType = 2;
            printf("Lumiere: SPOT\n");
            break;
        case ' ':
            foxAutoMove = !foxAutoMove;
            printf("Animation fox: %s\n", foxAutoMove ? "ACTIVE" : "PAUSE");
            break;
        case 's':
        case 'S':
            useShadersForScene = !useShadersForScene;
            printf("Shaders: %s\n", useShadersForScene ? "ACTIVES" : "DESACTIVES");
            break;
        case '0':
            light2On = !light2On;
            printf("Lumiere secondaire: %s\n", light2On ? "ON" : "OFF");
            break;
        case 'n':
        case 'N':
            autoDayNight = !autoDayNight;
            printf("Cycle jour/nuit: %s\n", autoDayNight ? "ACTIVE" : "PAUSE");
            break;
        case 'z':
        case 'Z':
            targetCameraDistance -= zoomSpeed * 2.0f;
            if (targetCameraDistance < 0.05f) targetCameraDistance = 0.05f;
            break;
        case 'd':
        case 'D':
            targetCameraDistance += zoomSpeed * 2.0f;
            if (targetCameraDistance > 25.0f) targetCameraDistance = 25.0f;
            break;
        case 'r':
        case 'R':
            targetCameraDistance = INITIAL_CAMERA_DISTANCE;
            cameraDistance = INITIAL_CAMERA_DISTANCE;
            cameraHeight = INITIAL_CAMERA_HEIGHT;
            cameraAngleX = INITIAL_CAMERA_ANGLE_X;
            cameraAngleY = INITIAL_CAMERA_ANGLE_Y;
            printf("RESET camera\n");
            break;
        case 27:
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    float moveSpeed = 0.3f;

    foxAutoMove = false;
    foxMovedManually = true;

    switch(key) {
        case GLUT_KEY_LEFT:
            foxX -= moveSpeed;
            foxAngle = 90.0f;
            foxLegAngle += 0.3f;
            foxMovedVertically = false;
            break;
        case GLUT_KEY_RIGHT:
            foxX += moveSpeed;
            foxAngle = -90.0f;
            foxLegAngle += 0.3f;
            foxMovedVertically = false;
            break;
        case GLUT_KEY_UP:
            foxZ -= moveSpeed;
            foxAngle = 180.0f;
            foxLegAngle += 0.3f;
            foxMovedVertically = true;
            break;
        case GLUT_KEY_DOWN:
            foxZ += moveSpeed;
            foxAngle = 0.0f;
            foxLegAngle += 0.3f;
            foxMovedVertically = true;
            break;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseRotating = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            mouseRotating = false;
        }
    }
}

void mouseMotion(int x, int y) {
    if (mouseRotating) {
        float deltaX = x - lastMouseX;
        float deltaY = y - lastMouseY;

        cameraAngleY += deltaX * 0.5f;
        cameraAngleX += deltaY * 0.5f;

        if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay();
    }
}

void mouseWheel(int button, int dir, int x, int y) {
    if (button == 3 || dir > 0) {
        targetCameraDistance -= 1.5f;
        if (targetCameraDistance < 0.05f) targetCameraDistance = 0.05f;
    } else if (button == 4 || dir < 0) {
        targetCameraDistance += 1.5f;
        if (targetCameraDistance > 25.0f) targetCameraDistance = 25.0f;
    }
    glutPostRedisplay();
}

// BOUCLE PRINCIPALE

void timer(int value) {
    timeElapsed += 0.016f;

    if (foxAutoMove && !foxMovedVertically) {
        foxX += foxSpeed * foxDirection;
        foxLegAngle += 0.08f;

        if (foxX > 8.0f) {
            foxDirection = -1;
            foxAngle = 90.0f;
        } else if (foxX < -8.0f) {
            foxDirection = 1;
            foxAngle = -90.0f;
        }
    }

    if (foxAutoMove && !foxMovedManually) {
        foxMovedVertically = false;
    }

    waterOffset += 0.04f;

    if (autoDayNight) {
        dayNightCycle += 0.01f;
    }

    float diff = targetCameraDistance - cameraDistance;
    if (fabs(diff) > 0.01f) {
        cameraDistance += diff * 0.25f;
    } else {
        cameraDistance = targetCameraDistance;
    }

    updateParticles();

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// MAIN

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("Timothy Treadwell : Katmai");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}

