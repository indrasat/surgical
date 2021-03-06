#include <stdlib.h>

#include "glThread.h"
#ifdef MAC
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <GL/gle.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/gle.h>
#endif

#include <iostream>
#include <fstream>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Cholesky>
#include <math.h>
#include "thread_discrete.h"
#include "trajectory_reader.h"
#include "thread_discrete_RRT.h"

// import most common Eigen types
USING_PART_OF_NAMESPACE_EIGEN

void InitLights();
void InitGLUT(int argc, char * argv[]);
void InitStuff();
void DrawStuff();
void DrawAxes();
void DrawRRT();
void InitThread(int argc, char* argv[]);

#define NUM_PTS 500
// #define THREAD_RADII 1.0
#define MOVE_POS_CONST 1.0
#define MOVE_TAN_CONST 0.2
#define ROTATE_TAN_CONST 0.2


enum key_code {NONE, MOVEPOS, MOVETAN, ROTATETAN};


float lastx_L=0;
float lasty_L=0;
float lastx_R=0;
float lasty_R=0;

float rotate_frame[2];
float move_end[2];
float tangent_end[2];
float tangent_rotation_end[2];

GLThread* glThreads[8];
int totalThreads = 8;
int curThread = 1;
int startThread = 0;
int planThread = 1;
int realThread = 2;
int endThread = 3;
int newRRTNodeThread = 6;

// double radii[NUM_PTS];
int pressed_mouse_button;

bool drawTree;
key_code key_pressed;


/* set up a light */
GLfloat lightOnePosition[] = {140.0, 0.0, 200.0, 0.0};
GLfloat lightOneColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightTwoPosition[] = {-140.0, 0.0, 200.0, 0.0};
GLfloat lightTwoColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightThreePosition[] = {140.0, 0.0, -200.0, 0.0};
GLfloat lightThreeColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightFourPosition[] = {-140.0, 0.0, -200.0, 0.0};
GLfloat lightFourColor[] = {0.99, 0.99, 0.99, 1.0};


void applyControl(Thread* start, const VectorXd& u, VectorXd* res) {
  int N = glThreads[realThread]->getThread()->num_pieces();
  res->setZero(3*N);

  Vector3d translation;
  translation << u(0), u(1), u(2);

  double dw = 1.0 - u(3)*u(3) - u(4)*u(4) - u(5)*u(5);
  if (dw < 0) {
    cout << "Huge differential quaternion: " << endl;
  }
  dw = (dw > 0) ? dw : 0.0;
  dw = sqrt(dw);
  Eigen::Quaterniond q(dw, u(3),u(4),u(5));
  Matrix3d rotation(q);

  // apply the control u to thread start, and return the new config in res
  Frame_Motion toMove(translation, rotation);

  Vector3d end_pos = start->end_pos();
  Matrix3d end_rot = start->end_rot();
  toMove.applyMotion(end_pos, end_rot);
  start->set_end_constraint(end_pos, end_rot);
  start->minimize_energy();

  start->toVector(res);
}

void computeDifference(Thread* a, Thread* b, VectorXd* res) {
  VectorXd avec;
  a->toVector(&avec);
  VectorXd bvec;
  b->toVector(&bvec);

  (*res) = avec - bvec;
}

void solveLinearizedControl() {
  // 2 hours
  // given planThread, endThread
  Thread* start = glThreads[planThread]->getThread();
  Thread* end   = glThreads[planThread]->getThread();

  // linearize the controls around the current thread (quasistatic, no dynamics)
  int N = glThreads[realThread]->getThread()->num_pieces();
  MatrixXd B(3*N,6);
  B.setZero();

  VectorXd du(6);
  double eps = 0.01;
  start->save_thread_pieces();
  // start->save_angle_twists();
  for(int i = 0; i < 6; i++) {
    du.setZero();
    du(i) = eps;
    VectorXd plus(3*N);
    start->restore_thread_pieces();
    // start->restore_angle_twists();
    applyControl(start, du, &plus);

    du(i) = -eps;
    VectorXd minus(3*N);
    start->restore_thread_pieces();
    // start->restore_angle_twists();
    applyControl(start, du, &minus);

    B.col(i) = (plus - minus) / (2*eps);
  }
  start->restore_thread_pieces();
  // start->restore_angle_twists();

  // solve the least-squares problem
  VectorXd dx;
  computeDifference(glThreads[endThread]->getThread(), glThreads[planThread]->getThread(), &dx);
  double C = 3.0;
  VectorXd u = B.transpose()*dx;
  (B.transpose()*B).llt().solveInPlace(u);
  // project it down to small step size
  u *= C/(B*u).squaredNorm();

  // apply the given control
  VectorXd x;
  applyControl(start, u, &x);
  glThreads[planThread]->updateThreadPoints();
  glutPostRedisplay();
}

void generateRandomThread() {
  // 1 hour
  // randomly stick out in a direction
  vector<Vector3d> vertices;
  vector<double> angles;
  int N = glThreads[realThread]->getThread()->num_pieces();


  vertices.push_back(Vector3d::Zero());
  angles.push_back(0.0);

  vertices.push_back(Vector3d::UnitX()*_rest_length);
  angles.push_back(0.0);

  double angle;
  Vector3d inc;
  Vector3d goal = glThreads[endThread]->getEndPosition() - glThreads[endThread]->getStartPosition();
  Vector3d noise;
  goal += (noise << Normal(0,1),Normal(0,1),Normal(0,1)).finished()*5.0;
  do {
    inc << Normal(0,1), Normal(0,1), Normal(0,1);
    //inc = goal;
    angle = acos(inc.dot(goal)/(goal.norm()*inc.norm()));
  } while(abs(angle) > M_PI/4.0);

  inc.normalize();
  inc *= _rest_length;

  for(int i = 0; i < N-2; i++) {
    vertices.push_back(vertices[vertices.size()-1] + inc);
    angles.push_back(0.0);
    // time to move toward the goal
    if ((vertices[vertices.size()-1] - goal).squaredNorm() > (N-2-i-1)*(N-2-i-1)*_rest_length*_rest_length) {
      inc = (goal - vertices[vertices.size()-1]).normalized()*_rest_length;
    }
  }

  Matrix3d rot = Matrix3d::Identity();
  glThreads[realThread]->setThread(new Thread(vertices, angles, rot));

  // better:
  // starting from 0,0, identity,
  // randomly sample links of thread given last link: maintain direction

  // minimize the energy on it
  glThreads[realThread]->minimize_energy();
  glThreads[realThread]->updateThreadPoints();
  glutPostRedisplay();
}

Thread_RRT planner;
vector<Frame_Motion> movements;
int curmotion = 0;
// 2 hours: implement rrt planner

// void planRRT() {
//   Thread* start = glThreads[planThread]->getThread();
//   Thread* end = glThreads[startThread]->getThread();

//   VectorXd s; VectorXd e;
//   start->toVector(&s);
//   end->toVector(&e);
//   cout << "start: " << endl << s << endl;
//   cout << "end: " << endl << e << endl;

//   if (curmotion >= movements.size()) {
//     movements.clear();
//     planner.planPath(start, end, movements);
//     curmotion = 0;
//     cout << "finished planning: path length: " << movements.size() << endl;
//     if (movements.size()==0) {
//       cout << "no plan found" << endl;
//       return;
//     }
//   }
//   Vector3d end_pos = start->end_pos();
//   Matrix3d end_rot = start->end_rot();
//   movements[curmotion].applyMotion(end_pos, end_rot);
//   curmotion++;
//   start->set_end_constraint(end_pos, end_rot);
//   start->minimize_energy();
//   glThreads[planThread]->updateThreadPoints();
//   glutPostRedisplay();
// }

bool initialized = false;
RRTNode* curNode;
void planRRT() {
  if (!initialized) {
    Thread* start = glThreads[planThread]->getThread();
    Thread* end = glThreads[endThread]->getThread();

    planner.initialize(start, end);
    initialized = true;
  }
  VectorXd goal_thread; VectorXd prev_thread; VectorXd next_thread;
  planner.planStep(&goal_thread, &prev_thread, &next_thread);
  curNode = planner.getTree()->front();

  // draw the goal thread, the previous closest, and the new added thread
  VectorXd angles(goal_thread.size()/3);
  angles.setZero();
  glThreads[4]->setThread(new Thread(goal_thread, angles, Matrix3d::Identity()));
  glThreads[4]->minimize_energy();
  glThreads[5]->setThread(new Thread(prev_thread, angles, Matrix3d::Identity()));
  glThreads[5]->minimize_energy();
  glThreads[6]->setThread(new Thread(next_thread, angles, Matrix3d::Identity()));
  glThreads[6]->minimize_energy();
  if (initialized) {
    glThreads[7]->setThread(new Thread(curNode->x, angles, Matrix3d::Identity()));
    glThreads[7]->minimize_energy();
  }
  glutPostRedisplay();
}

void planMovement() {
  // use quaternion interpolation to move closer to end rot
  Matrix3d end_rot = glThreads[planThread]->getEndRotation();

  // figure out angle between quats, spherically interpolate.
  Matrix3d goal_rot = glThreads[endThread]->getEndRotation();
  Eigen::Quaterniond endq(end_rot);
  Eigen::Quaterniond goalq(goal_rot);

  Vector3d after_goal = goal_rot*Vector3d::UnitX();
  Vector3d after_end = end_rot*Vector3d::UnitX();
  double angle = acos(after_goal.dot(after_end));
  double t = M_PI/128.0/angle;
  Eigen::Quaterniond finalq = endq.slerp(t, goalq).normalized();

  Matrix3d rotation(finalq*endq.inverse());


  // use linear interpolation to move closer to end pos
  Vector3d end_pos = glThreads[planThread]->getEndPosition();
  Vector3d goal_pos = glThreads[endThread]->getEndPosition();

  Vector3d translation = goal_pos - end_pos;
  double step = 2.0;
  translation.normalize();
  translation *= step;

  // apply the motion
  Frame_Motion toMove(translation, rotation);
  toMove.applyMotion(end_pos, end_rot);
  glThreads[planThread]->set_end_constraint(end_pos, end_rot);
  glThreads[planThread]->minimize_energy();
  glutPostRedisplay();
}

void selectThread(int inc) {
  curThread = (curThread + inc) % totalThreads;
  glutPostRedisplay();
}

void processLeft(int x, int y)
{
  if (key_pressed == MOVEPOS)
  {
    move_end[0] += (x-lastx_L)*MOVE_POS_CONST;
    move_end[1] += (lasty_L-y)*MOVE_POS_CONST;
  } else if (key_pressed == MOVETAN)
  {
    tangent_end[0] += (x-lastx_L)*MOVE_TAN_CONST;
    tangent_end[1] += (lasty_L-y)*MOVE_TAN_CONST;
  } else if (key_pressed == ROTATETAN)
  {
    tangent_rotation_end[0] += (x-lastx_L)*ROTATE_TAN_CONST;
    tangent_rotation_end[1] += (lasty_L-y)*ROTATE_TAN_CONST;
  }
  else {
    rotate_frame[0] += x-lastx_L;
    rotate_frame[1] += lasty_L-y;
  }

  lastx_L = x;
  lasty_L = y;
}

void processRight(int x, int y)
{
  //rotate_frame[0] += x-lastx_R;
  //rotate_frame[1] += y-lasty_R;

  if (key_pressed == MOVEPOS)
  {
    move_end[0] += (x-lastx_R)*MOVE_POS_CONST;
    move_end[1] += (lasty_R-y)*MOVE_POS_CONST;
  } else if (key_pressed == MOVETAN)
  {
    tangent_end[0] += (x-lastx_R)*MOVE_TAN_CONST;
    tangent_end[1] += (lasty_R-y)*MOVE_TAN_CONST;
  } else if (key_pressed == ROTATETAN)
  {
    tangent_rotation_end[0] += (x-lastx_R)*ROTATE_TAN_CONST;
    tangent_rotation_end[1] += (lasty_R-y)*ROTATE_TAN_CONST;
  }

  lastx_R = x;
  lasty_R = y;
}

void MouseMotion (int x, int y)
{
  if (pressed_mouse_button == GLUT_LEFT_BUTTON) {
    processLeft(x, y);
  } else if (pressed_mouse_button == GLUT_RIGHT_BUTTON) {
    processRight(x,y);
  }
  glutPostRedisplay ();
}

void processMouse(int button, int state, int x, int y)
{
  if (state == GLUT_DOWN)
  {
    pressed_mouse_button = button;
    if (button == GLUT_LEFT_BUTTON)
    {
      lastx_L = x;
      lasty_L = y;
    }
    if (button == GLUT_RIGHT_BUTTON)
    {
      lastx_R = x;
      lasty_R = y;
    }
    glutPostRedisplay ();
  }
}


void processSpecialKeys(int key, int x, int y) {
 if (key == GLUT_KEY_LEFT) {
   if(initialized) {
     if (curNode->prev != NULL) {
       curNode = curNode->prev;
       glThreads[7]->setThread(new Thread(curNode->x, VectorXd::Zero(curNode->N/3), Matrix3d::Identity()));
       glThreads[7]->minimize_energy();
       glutPostRedisplay();
     }
   }
 } else if (key == GLUT_KEY_RIGHT) {
   if(initialized) {
     if (curNode->next != NULL) {
       curNode = curNode->next;
       glThreads[7]->setThread(new Thread(curNode->x, VectorXd::Zero(curNode->N/3), Matrix3d::Identity()));
       glThreads[7]->minimize_energy();
       glutPostRedisplay();
     }
   }
 }
}

void processNormalKeys(unsigned char key, int x, int y)
{
  if (key == 't') {
    key_pressed = MOVETAN;
  }
  else if (key == 'm') {
    key_pressed = MOVEPOS;
  }
  else if (key == 'r') {
    key_pressed = ROTATETAN;
  }
  else if (key == 'l') {
    solveLinearizedControl();
  }
  else if (key == 'p') {
    planMovement();
  }
  else if (key == 'd') {
    generateRandomThread();
  } else if (key == 'a') {
    planRRT();
  }
  else if (key == 27)
  {
    exit(0);
  }

  lastx_R = x;
  lasty_R = y;

}

void processKeyUp(unsigned char key, int x, int y)
{
  if (key == '+') {
    selectThread(1);
  } else if (key == '-') {
    selectThread(-1);
  } else if (key == 'w') {
    drawTree = !drawTree;
    glutPostRedisplay ();
  }
  key_pressed = NONE;
  move_end[0] = move_end[1] = tangent_end[0] = tangent_end[1] = tangent_rotation_end[0] = tangent_rotation_end[1] = 0.0;
}



void JoinStyle (int msg)
{
  exit (0);
}


int main (int argc, char * argv[])
{

  srand(time(NULL));
  srand((unsigned int)time((time_t *)NULL));

  printf("Instructions:\n"
      "Hold down the left mouse button to rotate image: \n"
      "\n"
      "Hold 'm' while holding down the right mouse to move the end\n"
      "Hold 't' while holding down the right mouse to rotate the tangent \n"
      "\n"
      "Press 'Esc' to quit\n"
      );

  InitGLUT(argc, argv);
  InitLights();
  InitStuff ();



  InitThread(argc, argv);


  // for (int i=0; i < NUM_PTS; i++)
  // {
  //   radii[i]=THREAD_RADII;
  // }


  glutMainLoop ();
}


GLuint sphereList;
void InitStuff (void)
{
  glEnable(GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //gleSetJoinStyle (TUBE_NORM_PATH_EDGE | TUBE_JN_ANGLE );
  rotate_frame[0] = 0.0;
  rotate_frame[1] = -111.0;

  sphereList = glGenLists(1);
  glNewList(sphereList, GL_COMPILE);
  glutSolidSphere(0.5,16,16);
  glEndList();
}

/* draw the helix shape */
void DrawStuff (void)
{
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glColor3f (0.8, 0.3, 0.6);

  glPushMatrix ();

  /* set up some matrices so that the object spins with the mouse */
  glTranslatef (0.0,0.0,-150.0);
  glRotatef (rotate_frame[1], 1.0, 0.0, 0.0);
  glRotatef (rotate_frame[0], 0.0, 0.0, 1.0);


  //change thread, if necessary
  if (move_end[0] != 0.0 || move_end[1] != 0.0 || tangent_end[0] != 0.0 || tangent_end[1] != 0.0 || tangent_rotation_end[0] != 0 || tangent_rotation_end[1] != 0)
  {
    glThreads[curThread]->ApplyUserInput(move_end, tangent_end, tangent_rotation_end);
  }

  // draw planner end points
  if(drawTree) {
    DrawRRT();
  }


  //Draw Axes
  DrawAxes();



  for(int i = 0; i < totalThreads; i++) {
    glThreads[i]->DrawAxes();

    //Draw Thread
    if (i==curThread) {
      glColor3f (0.8, 0.4, 0.0);
    } else if (i==startThread) {
      glColor3f (0.2, 0.8, 0.2);
    } else if (i==endThread) {
      glColor3f (0.8, 0.2, 0.2);
    } else if (i==newRRTNodeThread) {
      glColor3f (0.2, 0.2, 0.8);
    } else if (i==7) {
      glColor3f (0.4, 0.4, 0.7);
    } else {
      glColor3f (0.5, 0.5, 0.1);
    }
    glThreads[i]->DrawThread();
  }

  glPopMatrix ();
  glutSwapBuffers ();
}



void InitThread(int argc, char* argv[])
{
  if (argc < 3) {
    for(int i = 0; i < totalThreads; i++) {
      glThreads[i] = new GLThread();
    }
  } else {
    Trajectory_Reader start_reader(argv[1]);
    start_reader.read_threads_from_file();
    Trajectory_Reader end_reader(argv[2]);
    end_reader.read_threads_from_file();

    glThreads[0] = new GLThread(new Thread(start_reader.get_all_threads()[0]));
    glThreads[1] = new GLThread(new Thread(start_reader.get_all_threads()[0]));
    glThreads[2] = new GLThread(new Thread(start_reader.get_all_threads()[0]));

    glThreads[3] = new GLThread(new Thread(end_reader.get_all_threads()[0]));

    glThreads[4] = new GLThread(new Thread(start_reader.get_all_threads()[0]));
    glThreads[5] = new GLThread(new Thread(start_reader.get_all_threads()[0]));
    glThreads[6] = new GLThread(new Thread(start_reader.get_all_threads()[0]));
    glThreads[7] = new GLThread(new Thread(start_reader.get_all_threads()[0]));

  }
  for(int i = 0; i < totalThreads; i++) {
    glThreads[i]->minimize_energy();
  }
}

void DrawRRT() {
  vector<RRTNode*>* tree = planner.getTree();

  glBegin(GL_LINES);
  glLineWidth(10.0);
  for(int i = 0; i < tree->size(); i++) {
    if(tree->at(i)->prev != NULL) {
      Vector3d loc = tree->at(i)->endPosition();
      Vector3d prev = tree->at(i)->prev->endPosition();
      glColor3d(1.0,1.0, 1.0);
      glVertex3f(prev(0),prev(1),prev(2));
      glVertex3f(loc(0),loc(1),loc(2));
    }
  }
  glEnd();

  glPushMatrix();
  glColor3f (1.0, 1.0, 1.0);
  for(int i = 0; i < tree->size(); i++) {
    Vector3d loc = tree->at(i)->endPosition();
    glTranslatef(loc(0),loc(1),loc(2));
    glCallList(sphereList);
    glTranslatef(-loc(0),-loc(1),-loc(2));
  }
  glPopMatrix();
}

void DrawAxes()
{
  //Draw Axes
  glBegin(GL_LINES);
  glEnable(GL_LINE_SMOOTH);
  glColor3d(1.0, 0.0, 0.0); //red
  glVertex3f(0.0f, 0.0f, 0.0f); //x
  glVertex3f(10.0f, 0.0f, 0.0f);
  glColor3d(0.0, 1.0, 0.0); //green
  glVertex3f(0.0f, 0.0f, 0.0f); //y
  glVertex3f(0.0f, 10.0f, 0.0f);
  glColor3d(0.0, 0.0, 1.0); //blue
  glVertex3f(0.0f, 0.0f, 0.0f); //z
  glVertex3f(0.0f, 0.0f, 10.0f);

  //label axes
  void * font = GLUT_BITMAP_HELVETICA_18;
  glColor3d(1.0, 0.0, 0.0); //red
  glRasterPos3i(20.0, 0.0, -1.0);
  glutBitmapCharacter(font, 'X');
  glColor3d(0.0, 1.0, 0.0); //red
  glRasterPos3i(0.0, 20.0, -1.0);
  glutBitmapCharacter(font, 'Y');
  glColor3d(0.0, 0.0, 1.0); //red
  glRasterPos3i(-1.0, 0.0, 20.0);
  glutBitmapCharacter(font, 'Z');
  glEnd();
}


void InitGLUT(int argc, char * argv[]) {
  /* initialize glut */
  glutInit (&argc, argv); //can i do that?
  glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(900,900);
  glutCreateWindow ("Thread");
  glutDisplayFunc (DrawStuff);
  glutMotionFunc (MouseMotion);
  glutMouseFunc (processMouse);
  glutKeyboardFunc(processNormalKeys);
  glutSpecialFunc(processSpecialKeys);
  glutKeyboardUpFunc(processKeyUp);

  /* create popup menu */
  glutCreateMenu (JoinStyle);
  glutAddMenuEntry ("Exit", 99);
  glutAttachMenu (GLUT_MIDDLE_BUTTON);

  /* initialize GL */
  glClearDepth (1.0);
  glEnable (GL_DEPTH_TEST);
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glShadeModel (GL_SMOOTH);

  glMatrixMode (GL_PROJECTION);
  /* roughly, measured in centimeters */
  glFrustum (-30.0, 30.0, -30.0, 30.0, 50.0, 500.0);
  glMatrixMode(GL_MODELVIEW);
}


void InitLights() {
  /* initialize lighting */
  glLightfv (GL_LIGHT0, GL_POSITION, lightOnePosition);
  glLightfv (GL_LIGHT0, GL_DIFFUSE, lightOneColor);
  glEnable (GL_LIGHT0);
  glLightfv (GL_LIGHT1, GL_POSITION, lightTwoPosition);
  glLightfv (GL_LIGHT1, GL_DIFFUSE, lightTwoColor);
  glEnable (GL_LIGHT1);
  glLightfv (GL_LIGHT2, GL_POSITION, lightThreePosition);
  glLightfv (GL_LIGHT2, GL_DIFFUSE, lightThreeColor);
  glEnable (GL_LIGHT2);
  glLightfv (GL_LIGHT3, GL_POSITION, lightFourPosition);
  glLightfv (GL_LIGHT3, GL_DIFFUSE, lightFourColor);
  glEnable (GL_LIGHT3);
  glEnable (GL_LIGHTING);
  glColorMaterial (GL_FRONT_AND_BACK, GL_DIFFUSE);
  glEnable (GL_COLOR_MATERIAL);
}
