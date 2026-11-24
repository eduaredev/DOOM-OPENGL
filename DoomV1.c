#include <stdio.h>
#include <stdlib.h>
#include <GL/freeglut.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#define PI 3.14159265358979f

#define DU (1.0f / 32.0f) // Unidad DOOM, 1 unidad es un pixel, 32 unidades DOOM es 1 unidad
#define ESPACIO 16.0F
#define TECHO 2.0f
#define PARED 4.0f
#define ALTURA_OJO (41.0f * DU)    // Altura del jugador
#define RADIO_JUGADOR (16.0f * DU) // Radio del judador
#define VELOCIDAD_MOV 7.0f
#define VELOCIDAD_GIRO 2.5f
#define SENS_MOUSE 0.004f

#define MAX_BARRELS 16
#define BARREL_R (10.0f * DU)

#define MOV_VEL_TIC 35.0f // Recorre 35 tics o pixeles por segundo

typedef struct
{
    GLuint tex;
    int w, h;
    int ox, oy;
    int ok;
} Sprite;

typedef enum
{
    W_READY,
    W_FIRE1,
    W_FIRE2,
    W_FIRE3,
    W_FIRE4
} WState;

static WState wState = W_READY;
static float wTics = 0;
static float flashTics = 0;

static int winW = 960, winH = 600;
static float px = 0.0f, pz = 8.0f;
static float pAngle = 0.0f;
static int keys[256];
static int special[256];
static int lastTime = 0;

static float balanceo = 0.0f;
static float cant_balanceo = 0.0f;
static int fireHeld, mouseFire;

static int CapturaMouse = 1;

#define CARGADOR 12
#define T_BAJAR 0.25f  // segundos bajando el arma
#define T_CAMBIO 0.35f // segundos con el arma abajo
#define T_SUBIR 0.25f  // segundos subiendo el arma
static int balas = CARGADOR;
static int recargando = 0;
static float tRecarga = 0.0f;

static void color3(int r, int g, int b);
static void dibujartyp(void);
static void dibujarporsegmentos(float ax, float az, float bx, float bz, int r, int g, int b);
static void display(void);
static void update(void);
static void keyDown(unsigned char k, int x, int y);
static void keyUp(unsigned char k, int x, int y);
static void specDown(int k, int x, int y);
static void specUp(int k, int x, int y);
static void mouseMove(int x, int y);
static void reshape(int w, int h);
static Sprite loadSprite(const char *name, int defOx, int defOy);
static int readGrab(const char *path, int *ox, int *oy);
static Sprite sPisA, sPisB, sPisC, sFlash;
static void drawPSprite(const Sprite *s, float sx, float sy);
static void tickWeapon(float tics);
static void setWeapon(WState s);
static void rect(float x0, float y0, float x1, float y1);
static void ellipse(float cx, float cy, float rx, float ry);
static void drawShapeHand(float ox, float oy, int flash);
static void quad(float a, float b, float c, float d, float e, float f, float g, float h);
static void loadAssets(void);
static void dibujarArma(void);
static void iniciarRecarga(void);
static void tickRecarga(float dt);
static void mouseButton(int button, int state, int x, int y);

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("DOOM - OpenGL");

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);

    loadAssets();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(update);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specDown);
    glutSpecialUpFunc(specUp);
    glutPassiveMotionFunc(mouseMove);
    glutMotionFunc(mouseMove);
    glutMouseFunc(mouseButton);
    glutIgnoreKeyRepeat(1);
    glutSetCursor(GLUT_CURSOR_NONE);

    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    return 0;
}

static void setWeapon(WState s)
{
    // Duracion del arma
    static const int dur[] = {0, 4, 6, 4, 5};
    wState = s;
    wTics = (float)dur[s];
    if (s == W_FIRE2)
    {
        flashTics = 7;
        balas--;
    }
}

static void tickWeapon(float tics)
{
    if (flashTics > 0)
        flashTics -= tics;
    if (wState == W_READY)
    {
        if (fireHeld && !recargando)
        {
            if (balas > 0)
                setWeapon(W_FIRE1);
            else
                iniciarRecarga();
        }
        return;
    }
    wTics -= tics;
    if (wTics <= 0)
    {
        if (wState == W_FIRE4)
        {
            if (fireHeld && balas > 0)
                setWeapon(W_FIRE1);
            else
                wState = W_READY;
        }
        else
            setWeapon((WState)(wState + 1));
    }
}

static void drawPSprite(const Sprite *s, float sx, float sy)
{
    float x = sx - s->ox, y = sy - s->oy;
    glBindTexture(GL_TEXTURE_2D, s->tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + s->w, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + s->w, y + s->h);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + s->h);
    glEnd();
}

static int readGrab(const char *path, int *ox, int *oy)
{
    unsigned char buf[8];
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fseek(f, 8, SEEK_SET);
    while (fread(buf, 1, 8, f) == 8)
    {
        unsigned len = (unsigned)buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
        if (memcmp(buf + 4, "grAb", 4) == 0 && len == 8)
        {
            unsigned char d[8];
            if (fread(d, 1, 8, f) != 8)
                break;
            *ox = (int)((unsigned)d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3]);
            *oy = (int)((unsigned)d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]);
            fclose(f);
            return 1;
        }
        if (memcmp(buf + 4, "IDAT", 4) == 0)
            break;
        fseek(f, (long)len + 4, SEEK_CUR);
    }
    fclose(f);
    return 0;
}

static Sprite loadSprite(const char *name, int defOx, int defOy)
{
    Sprite s;
    char path[256];
    int n;
    unsigned char *img;

    memset(&s, 0, sizeof s);
    snprintf(path, sizeof path, "sprites/%s.png", name);
    img = stbi_load(path, &s.w, &s.h, &n, 4);
    if (!img)
    {
        fprintf(stderr, "No encontre %s (uso figuras)\n", path);
        return s;
    }

    s.ox = defOx;
    s.oy = defOy;
    readGrab(path, &s.ox, &s.oy);

    glGenTextures(1, &s.tex);
    glBindTexture(GL_TEXTURE_2D, s.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s.w, s.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    s.ok = 1;
    return s;
}

static void reshape(int w, int h)
{
    winW = w;
    winH = h > 0 ? h : 1;
    glViewport(0, 0, winW, winH);
}

static void update(void)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) / 1000.0f, fx, fz, rx, rz, mx = 0, mz = 0, len, tics;
    lastTime = now;
    if (dt > 0.1f)
        dt = 0.1f;

    if (special[GLUT_KEY_LEFT] || keys['q'])
        pAngle -= VELOCIDAD_GIRO * dt;
    if (special[GLUT_KEY_RIGHT] || keys['e'])
        pAngle += VELOCIDAD_GIRO * dt;
    fx = sinf(pAngle);
    fz = -cosf(pAngle);
    rx = cosf(pAngle);
    rz = sinf(pAngle);
    if (keys['w'] || special[GLUT_KEY_UP])
    {
        mx += fx;
        mz += fz;
    }
    if (keys['s'] || special[GLUT_KEY_DOWN])
    {
        mx -= fx;
        mz -= fz;
    }
    if (keys['d'])
    {
        mx += rx;
        mz += rz;
    }
    if (keys['a'])
    {
        mx -= rx;
        mz -= rz;
    }

    len = sqrtf(mx * mx + mz * mz);
    if (len > 0.001f)
    {
        px += mx / len * VELOCIDAD_MOV * dt;
        pz += mz / len * VELOCIDAD_MOV * dt;
        balanceo += dt * 9.0f;
        cant_balanceo += dt * 4.0f;
        if (cant_balanceo > 1)
            cant_balanceo = 1;
    }
    else
    {
        cant_balanceo -= dt * 4.0f;
        if (cant_balanceo < 0)
            cant_balanceo = 0;
    }

    tics = dt * MOV_VEL_TIC;
    fireHeld = keys[' '] || mouseFire;
    tickWeapon(tics);
    tickRecarga(dt);
    glutPostRedisplay();
}

static void loadAssets(void)
{
    sPisA = loadSprite("pisga0", -134, -100);
    sPisB = loadSprite("pisgb0", -133, -92);
    sPisC = loadSprite("pisgc0", -133, -92);
    sFlash = loadSprite("pisfa0", -144, -79);
}

static void keyDown(unsigned char k, int x, int y)
{
    (void)x;
    (void)y;
    if (k == 27)
        exit(0);
    if (k >= 'A' && k <= 'Z')
        k = k - 'A' + 'a';
    if (k == 'r')
        iniciarRecarga();
    keys[k] = 1;
}
static void keyUp(unsigned char k, int x, int y)
{
    (void)x;
    (void)y;
    if (k >= 'A' && k <= 'Z')
        k = k - 'A' + 'a';
    keys[k] = 0;
}

static void specDown(int k, int x, int y)
{
    (void)x;
    (void)y;
    if (k < 256)
        special[k] = 1;
}
static void specUp(int k, int x, int y)
{
    (void)x;
    (void)y;
    if (k < 256)
        special[k] = 0;
}

static void mouseMove(int x, int y)
{
    int cx = winW / 2, cy = winH / 2;
    (void)y;
    if (!CapturaMouse)
        return;
    if (x == cx && y == cy)
        return;
    pAngle += (x - cx) * SENS_MOUSE;
    glutWarpPointer(cx, cy);
}

static void display(void)
{
    float lx = sinf(pAngle), lz = -cosf(pAngle);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(65.0, (double)winW / winH, 0.05, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(px, ALTURA_OJO, pz, px + lx, ALTURA_OJO, pz + lz, 0, 1, 0);

    dibujartyp();
    dibujarporsegmentos(-ESPACIO, -ESPACIO, ESPACIO, -ESPACIO, 40, 40, 48);
    dibujarporsegmentos(ESPACIO, -ESPACIO, ESPACIO, ESPACIO, 40, 40, 48);
    dibujarporsegmentos(ESPACIO, ESPACIO, -ESPACIO, ESPACIO, 40, 40, 48);
    dibujarporsegmentos(-ESPACIO, ESPACIO, -ESPACIO, -ESPACIO, 40, 40, 48);

    dibujarArma();
    glutSwapBuffers();
}

static void dibujartyp(void)
{
    int n = (int)(ESPACIO * 2 / TECHO);
    glBegin(GL_QUADS);
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float x0 = -ESPACIO + i * TECHO, x1 = x0 + TECHO;
            float z0 = -ESPACIO + j * TECHO, z1 = z0 + TECHO;
            if ((i + j) & 1)

                color3(48, 48, 56);
            else
                color3(40, 40, 48);
            glVertex3f(x0, 0, z0);
            glVertex3f(x1, 0, z0);
            glVertex3f(x1, 0, z1);
            glVertex3f(x0, 0, z1);

            if ((i + j) & 1)
                color3(48, 48, 56);
            else
                color3(40, 40, 48);
            glVertex3f(x0, PARED, z0);
            glVertex3f(x0, PARED, z1);
            glVertex3f(x1, PARED, z1);
            glVertex3f(x1, PARED, z0);
        }
    }
    glEnd();
}

static void dibujarporsegmentos(float ax, float az, float bx, float bz, int r, int g, int b)
{
    float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
    int segs = (int)(len / TECHO), row;
    int rows = 3;
    float rowH = PARED / rows;

    glBegin(GL_QUADS);
    for (int s = 0; s < segs; s++)
    {
        float t0 = (float)s / segs, t1 = (float)(s + 1) / segs;
        float x0 = ax + (bx - ax) * t0, z0 = az + (bz - az) * t0;
        float x1 = ax + (bx - ax) * t1, z1 = az + (bz - az) * t1;
        for (row = 0; row < rows; row++)
        {
            float y0 = row * rowH, y1 = y0 + rowH;
            int k = ((s + row) & 1) ? 0 : 18;
            if (row == 0)
                k -= 25;
            color3(r + k < 0 ? 0 : r + k, g + k < 0 ? 0 : g + k, b + k < 0 ? 0 : b + k);
            glVertex3f(x0, y0, z0);
            glVertex3f(x1, y0, z1);
            glVertex3f(x1, y1, z1);
            glVertex3f(x0, y1, z0);
        }
    }

    glEnd();
}

static void color3(int r, int g, int b)
{
    glColor3ub(r, g, b);
}

static void rect(float x0, float y0, float x1, float y1)
{
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}
static void quad(float a, float b, float c, float d, float e, float f, float g, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(a, b);
    glVertex2f(c, d);
    glVertex2f(e, f);
    glVertex2f(g, h);
    glEnd();
}
static void ellipse(float cx, float cy, float rx, float ry)
{
    int i;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (i = 0; i <= 20; i++)
    {
        float a = i / 20.0f * 2 * PI;
        glVertex2f(cx + cosf(a) * rx, cy + sinf(a) * ry);
    }
    glEnd();
}
static void drawShapeHand(float ox, float oy, int flash)
{
    glPushMatrix();
    glTranslatef(ox, oy, 0);
    color3(40, 70, 30);
    quad(128, 200, 192, 200, 186, 178, 134, 178);
    color3(170, 110, 75);
    quad(140, 180, 180, 180, 177, 160, 143, 160);
    color3(70, 70, 78);
    quad(147, 150, 173, 150, 168, 102, 152, 102);
    color3(45, 45, 52);
    quad(147, 150, 152, 150, 155, 102, 152, 102);
    color3(105, 105, 115);
    quad(168, 150, 173, 150, 168, 102, 165, 102);
    color3(0, 0, 0);
    rect(157, 101, 163, 105);
    color3(190, 125, 85);
    ellipse(160, 160, 22, 12);
    color3(205, 140, 98);
    ellipse(147, 153, 5, 4);
    ellipse(156, 151, 5, 4);
    ellipse(165, 151, 5, 4);
    ellipse(174, 153, 5, 4);
    color3(185, 120, 80);
    quad(138, 162, 146, 156, 152, 140, 147, 140);
    if (flash)
    {
        color3(255, 200, 60);
        ellipse(160, 92, 14, 11);
        color3(255, 255, 220);
        ellipse(160, 92, 6, 5);
    }
    glPopMatrix();
}

static void mouseButton(int button, int state, int x, int y)
{
    (void)x;
    (void)y;
    if (button == GLUT_LEFT_BUTTON)
        mouseFire = (state == GLUT_DOWN);
}

static void iniciarRecarga(void)
{
    if (recargando || balas == CARGADOR || wState != W_READY)
        return;
    recargando = 1;
    tRecarga = 0.0f;
}

static void tickRecarga(float dt)
{
    if (!recargando)
        return;
    tRecarga += dt;
    if (balas < CARGADOR && tRecarga >= T_BAJAR + T_CAMBIO * 0.5f)
        balas = CARGADOR;
    if (tRecarga >= T_BAJAR + T_CAMBIO + T_SUBIR)
        recargando = 0;
}

static void offsetRecarga(float *bajar, float *girar)
{
    float t = tRecarga, k;
    *bajar = 0.0f;
    *girar = 0.0f;
    if (!recargando)
        return;
    if (t < T_BAJAR)
        k = t / T_BAJAR;
    else if (t < T_BAJAR + T_CAMBIO)
        k = 1.0f;
    else
        k = 1.0f - (t - T_BAJAR - T_CAMBIO) / T_SUBIR;
    if (k < 0)
        k = 0;
    k = k * k * (3 - 2 * k);
    *bajar = 70.0f * k;
    *girar = 25.0f * k;
    if (t > T_BAJAR + T_CAMBIO * 0.5f && t < T_BAJAR + T_CAMBIO * 0.5f + 0.08f)
        *bajar += 6.0f;
}

static void dibujarArma(void)
{
    float bob = 8.0f * cant_balanceo;
    float sx = 1.0f + cosf(balanceo) * bob;
    float sy = 32.0f + fabsf(sinf(balanceo)) * bob;
    float bajar, girar;
    const Sprite *frame;

    offsetRecarga(&bajar, &girar);
    sy += bajar;

    if (wState == W_READY)
        frame = &sPisA;
    else if (wState == W_FIRE2)
        frame = &sPisC;
    else
        frame = &sPisB;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 320, 200, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    glTranslatef(200.0f, 200.0f, 0.0f);
    glRotatef(girar, 0, 0, 1);
    glTranslatef(-200.0f, -200.0f, 0.0f);

    if (sPisA.ok && sPisB.ok && sPisC.ok)
    {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor3f(1, 1, 1);
        drawPSprite(frame, sx, sy);
        if (flashTics > 0 && sFlash.ok)
            drawPSprite(&sFlash, sx, sy);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        drawShapeHand(sx - 1, sy - 28 - (wState == W_FIRE2 ? 6 : 0), flashTics > 0);
    }

    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
