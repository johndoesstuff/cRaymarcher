#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include <termios.h>
#define clrscr() printf("\e[1;1H\e[2J")
#define EPSILON 1e-5

void normalize(float *a) {
        float scale = sqrt(pow(a[0], 2) + pow(a[1], 2) + pow(a[2], 2));
        a[0] = a[0]/scale;
        a[1] = a[1]/scale;
        a[2] = a[2]/scale;
}

float sigmoid(float x) {
        return 1.0/(1.0 + exp(-x));
}

float squishRange(int n, int screen) {
        return 2.0*((1.0*n)/(1.0*screen)) - 1;
}

void scaleVec(float *a, float s) {
        a[0] = a[0]*s;
        a[1] = a[1]*s;
        a[2] = a[2]*s;
}

void addVec(float *v0, float *v1) {
        v0[0] += v1[0];
        v0[1] += v1[1];
        v0[2] += v1[2];
}

float dot(float *a, float *b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void modVec(float *a, float v) {
        a[0] = fmod(v + fmod(a[0], v), v);
        a[1] = fmod(v + fmod(a[1], v), v);
        a[2] = fmod(v + fmod(a[2], v), v);
}

void rotXVec(float *a, float dir) {
        float tmp[3] = { a[0], a[1], a[2] };
        a[1] = cos(dir)*tmp[1] - sin(dir)*tmp[2];
        a[2] = sin(dir)*tmp[1] + cos(dir)*tmp[2];
}

void rotYVec(float *a, float dir) {
        float tmp[3] = { a[0], a[1], a[2] };
        a[0] = cos(dir)*tmp[0] + sin(dir)*tmp[2];
        a[2] = -sin(dir)*tmp[0] + cos(dir)*tmp[2];
}

void rotZVec(float *a, float dir) {
        float tmp[3] = { a[0], a[1], a[2] };
        a[0] = cos(dir)*tmp[0] - sin(dir)*tmp[1];
        a[1] = sin(dir)*tmp[0] + cos(dir)*tmp[1];
}

void rot3Vec(float *a, float *dir) {
        rotXVec(a, dir[0]);
        rotYVec(a, dir[1]);
        rotZVec(a, dir[2]);
}

float sphereSDF(float *vec, float *pos, float r) {
        return sqrt(pow(vec[0] - pos[0], 2) + pow(vec[1] - pos[1], 2) + pow(vec[2] - pos[2], 2)) - r;
}

float torusSDF(float *vec, float *pos, float *t) {
        float q[2] = {sqrt(pow(vec[0] - pos[0], 2) + pow(vec[2] - pos[2], 2)) - t[0], vec[1] - pos[1]};
        return sqrt(pow(q[0], 2) + pow(q[1], 2)) - t[1];
}

float planeSDF(float *vec, float *p, float h) {
        return dot(vec, p) + h;
}

float SDF(float *a) {
        float tmp[3] = { a[0], a[1], a[2] };
        /*float spherePos[3] = { 4.0, 4.0, 4.0 };
        modVec(tmp, 8);
        return sphereSDF(tmp, spherePos, 2.0);*/

        float torusPos[3] = { 4.0, 4.0, 4.0 };
        float torusT[2] = { 2.5, 1.0 };
        modVec(tmp, 9.0);
        return torusSDF(tmp, torusPos, torusT);

        /*float spherePos[3] = { -1.0, 0.0, 0.0 };
        float plane[3] = { 1.0, 0.0, 0.0 };
        return fmin(sphereSDF(tmp, spherePos, 4.0), planeSDF(tmp, plane, 0.0));*/
}

void normal(float *pos, float *out) {
        float normalEpsilon = 1e-6;
        float tmp0[3] = { pos[0] + normalEpsilon, pos[1], pos[2] };
        float tmp1[3] = { pos[0] - normalEpsilon, pos[1], pos[2] };
        out[0] = SDF(tmp0) - SDF(tmp1);
        float tmp2[3] = { pos[0], pos[1] + normalEpsilon, pos[2] };
        float tmp3[3] = { pos[0], pos[1] - normalEpsilon, pos[2] };
        out[1] = SDF(tmp2) - SDF(tmp3);
        float tmp4[3] = { pos[0], pos[1], pos[2] + normalEpsilon };
        float tmp5[3] = { pos[0], pos[1], pos[2] - normalEpsilon };
        out[2] = SDF(tmp4) - SDF(tmp5);
        normalize(out);
}

void render(int frame, float *cameraAngle, float *cameraPosition, float *lightPosition) {
        struct winsize ws;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
                perror("ioctl");
                exit(EXIT_FAILURE);
        }

        int screenX = ws.ws_col;
        int screenY = ws.ws_row-2;

        int iterations = 360;
        float pixels[screenY][screenX];
        float viewVectors[screenY][screenX][3];
        //float cameraAngle[3] = { frame/267.0, frame/521.0, frame/100.0 };
        //float cameraAngle[3] = { 0.0, 0.0, 0.0 };
        //float sceneAngle[3] = { frame/67.0, frame/21.0, frame/100.0 };
        float sceneAngle[3] = { 0.0, 0.0, 0.0 };
        for (int y = 0; y < screenY; y++) {
                for (int x = 0; x < screenX; x++) {
                        viewVectors[y][x][0] = squishRange(x, screenX);
                        viewVectors[y][x][1] = squishRange(y, screenY)/2;
                        viewVectors[y][x][2] = 1;

                        normalize(viewVectors[y][x]);

                        float normalizedVector[3];
                        normalizedVector[0] = viewVectors[y][x][0];
                        normalizedVector[1] = viewVectors[y][x][1];
                        normalizedVector[2] = viewVectors[y][x][2];

                        rot3Vec(normalizedVector, cameraAngle);

                        float magnitudeVector[3];
                        float positionVector[3];

                        positionVector[0] = cameraPosition[0];
                        positionVector[1] = cameraPosition[1];
                        positionVector[2] = cameraPosition[2];

                        rot3Vec(positionVector, sceneAngle);
                        rot3Vec(normalizedVector, sceneAngle);

                        float s;
                        float dist = 0;

                        _Bool escape = 0;

                        for (int i = 0; (i < iterations) && !escape; i++) {
                                s = SDF(positionVector);
                                dist += s;
                                magnitudeVector[0] = normalizedVector[0];
                                magnitudeVector[1] = normalizedVector[1];
                                magnitudeVector[2] = normalizedVector[2];

                                scaleVec(magnitudeVector, s);
                                addVec(positionVector, magnitudeVector);

                                escape = s < EPSILON || s > 1e6;
                        }

                        _Bool hit = s < EPSILON;

                        float surfaceNormal[3];
                        normal(positionVector, surfaceNormal);

                        float lightPositionVector[3] = { positionVector[0], positionVector[1], positionVector[2] };
                        float lightDist = 0;

                        float lightNormalizedVector[3];
                        lightNormalizedVector[0] = -lightPosition[0]+positionVector[0];
                        lightNormalizedVector[1] = -lightPosition[1]+positionVector[1];
                        lightNormalizedVector[2] = -lightPosition[2]+positionVector[2];

                        normalize(lightNormalizedVector);

                        _Bool lightHit = 0;

                        //rot3Vec(normalizedVector, cameraAngle);

                        if (hit) {
                                float proximity;
                                for (int i = 0; (i < iterations) && !escape; i++) {
                                        proximity = sqrt(pow(lightPositionVector[0] - lightPosition[0], 2) + pow(lightPositionVector[1] - lightPosition[1], 2) + pow(lightPositionVector[2] - lightPositionVector[2], 2));
                                        s = fmin(SDF(lightPositionVector), proximity);
                                        lightDist += s;
                                        magnitudeVector[0] = lightNormalizedVector[0];
                                        magnitudeVector[1] = lightNormalizedVector[1];
                                        magnitudeVector[2] = lightNormalizedVector[2];

                                        scaleVec(magnitudeVector, s);
                                        addVec(lightPositionVector, magnitudeVector);

                                        escape = (s < EPSILON || s > 1e6 || proximity < EPSILON) && i > 5;
                                }

                                if (proximity < EPSILON) {
                                        lightHit = 1;
                                }

                        }

                        //pixels[y][x] = hit ? ((1 - (2*sigmoid(dist / 20.0)-1)) * (-dot(normalizedVector, surfaceNormal))) : -1;
                        pixels[y][x] = hit ? ((1 - (2*sigmoid(dist / 20.0)-1)) * (-dot(lightNormalizedVector, surfaceNormal))) : -1;
                        pixels[y][x] *= (1 - (2*sigmoid(lightDist / 20.0)-1));
                        pixels[y][x] *= lightHit;
                        //pixels[y][x] *= 0.8*lightHit + 0.2;

                        //debug
                        if (x == 10 && y == 9 && 0) {
                                printf("Vector at %d %d is %f %f %f\n", x, y, viewVectors[y][x][0], viewVectors[y][x][1], viewVectors[y][x][2]);
                                printf("SDF %f\n", s);
                                printf(hit ? "Hit: true\n" : "Hit: false\n");
                        }
                }
        }

        //render pixels

        char buffer[1000000];
        buffer[0] = '\0';
        size_t offset = 0;

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "W: %d, H: %d\n", ws.ws_col, ws.ws_row);
        for (int y = 0; y < screenY; y++) {
                for (int x = 0; x < screenX; x++) {
                        int brightness = 232; //brightness is on a scale from 232 to 255
                        float theoreticalBrightness = 0;
                        if (pixels[y][x] >= 0) {
                                theoreticalBrightness = pixels[y][x];
                                theoreticalBrightness *= 23*3;
                                brightness = 232 + theoreticalBrightness/3;
                        }
                        if (fmod(theoreticalBrightness, 3) < 1) {
                                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\033[48;5;%dm\033[38;5;%dm \033[0m", brightness, brightness);
                        } else if (fmod(theoreticalBrightness, 3) < 2) {
                                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\033[48;5;%dm\033[38;5;%dm#\033[0m", brightness, brightness + 1);
                        } else {
                                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\033[48;5;%dm\033[38;5;%dm#\033[0m", brightness + 1, brightness);
                        }
                }
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
        }

        //clrscr();

        printf("\033[H");

        printf("\033[?25l");

        printf("%s", buffer);
        printf("\033[?25h");
        //printf("%d", (int)sizeof(buffer));

        fflush(stdout);
}

int main() {
        int f = 0;

        float cameraAngle[3] = { 0.0, 0.0, 0.0 };
        float cameraPosition[3] = { 0.0, 0.0, -10.0 };

        float lightPosition[3] = { 0.0, 0.0, 0.0 };

        while (1) {
                char keys[32];

                cameraPosition[0] += sin(f / 100.0)/5.0;
                cameraPosition[1] += sin(f / 123.1)/7.0;
                cameraPosition[2] += sin(f / 167.3)/9.0;

                cameraAngle[0] = f / 178.0;
                cameraAngle[1] = f / 145.4;
                cameraAngle[2] = f / 267.1;

                lightPosition[0] = cameraPosition[0] + 4*sin(f / 20.0);
                lightPosition[1] = cameraPosition[1] + 4*sin(f / 30.0 + 5);
                lightPosition[2] = cameraPosition[2] + 4*sin(f / 50.0 + 9);

                //lightPosition[2] = f / 20.0;

                render(f, cameraAngle, cameraPosition, lightPosition);
                f++;
                usleep(50000);
        }
}