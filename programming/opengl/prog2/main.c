#include <GL/glut.h>

void display(void)
{
  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);

  glBegin(GL_TRIANGLES);
  {
    glColor3f(1,0,0);
    glVertex2f(0,0);

    glColor3f(0,1,0);
    glVertex2f(.5,0);

    glColor3f(0,0,1);
    glVertex2f(.5,.5);
  }
  glEnd();
  glutSwapBuffers();
}

void reshape(int w, int h)
{
}

int main(int argc, char **argv) {
  glutInit(&argc, argv);
  
  glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);  
  glutInitWindowPosition(200,200);
  glutInitWindowSize(512,512);
  glutCreateWindow("Part 1");
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  
  glutMainLoop();
  return 0;
}

