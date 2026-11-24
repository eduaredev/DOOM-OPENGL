#include <stdio.h>
#include <stdlib.h>
#include <GL/freeglut.h>
#include <string.h>
#include <math.h>

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

#define MOV_VEL_TIC 35.0f // Recorre 35 tics o pixeles por segundo

static int winW = 960, winH = 600;
static float px = 0.0f, pz = 8.0f;
static float pAngle = 0.0f;
static int keys[256];
static int special[256];
static int lastTime = 0;

static float balanceo = 0.0f;
static float cant_balanceo = 0.0f;

static int CapturaMouse = 1;

static void color3(int r, int g, int b);
static void dibujartyp(void);
static void dibujarporsegmentos(float ax, float az, float bx, float bz, int r, int g, int b);
static void dibujarparedes(void);
static void display(void);
static void update(void);
static void keyDown(unsigned char k, int x, int y);
static void keyUp(unsigned char k, int x, int y);
static void specDown(int k, int x, int y);
static void specUp(int k, int x, int y);
static void mouseMove(int x, int y);
static void reshape(int w, int h);

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("DOOM - OpenGL");

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(update);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specDown);
    glutSpecialUpFunc(specUp);
    glutPassiveMotionFunc(mouseMove);
    glutMotionFunc(mouseMove);
    glutIgnoreKeyRepeat(1);
    glutSetCursor(GLUT_CURSOR_NONE);

    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    return 0;
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
    float dt = (now - lastTime) / 1000.0f;
    float fx, fz, rx, rz, mx = 0, mz = 0, len;
    float lim = ESPACIO - RADIO_JUGADOR;
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
        mx /= len;
        mz /= len;
        px += mx * VELOCIDAD_MOV * dt;
        pz += mz * VELOCIDAD_MOV * dt;
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

    /* colision simple contra las 4 paredes */
    if (px > lim)
        px = lim;
    if (px < -lim)
        px = -lim;
    if (pz > lim)
        pz = lim;
    if (pz < -lim)
        pz = -lim;

    glutPostRedisplay();
}

static void keyDown(unsigned char k, int x, int y)
{
    (void)x;
    (void)y;
    if (k == 27)
        exit(0);
    if (k >= 'A' && k <= 'Z')
        k = k - 'A' + 'a';
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
    gluPerspective(70.0, (double)winW / winH, 0.05, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(px, ALTURA_OJO, pz, px + lx, ALTURA_OJO, pz + lz, 0, 1, 0);

    dibujartyp();
    dibujarparedes();

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

static void dibujarparedes(void)
{
    float h = ESPACIO;

    dibujarporsegmentos(-h, -h, h, -h, 40, 40, 48);
    dibujarporsegmentos(h, -h, h, h, 40, 40, 48);
    dibujarporsegmentos(h, h, -h, h, 40, 40, 48);
    dibujarporsegmentos(-h, h, -h, -h, 40, 40, 48);
}

static void color3(int r, int g, int b)
{
    glColor3ub(r, g, b);
}
