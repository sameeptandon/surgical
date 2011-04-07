#include <stdlib.h>

#include "../DiscreteRods/glThread.h"
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
#include <signal.h>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Cholesky>
#include <math.h>
#include "../DiscreteRods/thread_discrete.h"
#include "../DiscreteRods/trajectory_reader.h"
#include "../DiscreteRods/trajectory_recorder.h"
#include "planner_utils.h"
#include "linearization_utils.h"
#include "trajectory_follower.h"

// import most common Eigen types
USING_PART_OF_NAMESPACE_EIGEN

void InitLights();
void InitGLUT(int argc, char * argv[]);
void InitStuff();
void DrawStuff();
void DrawAxes();
void DrawRRT();
void InitThread(int argc, char* argv[]);

#define MOVE_POS_CONST 1.0
#define MOVE_TAN_CONST 0.2
#define ROTATE_TAN_CONST 0.2
#define RRT_GOAL_THREAD_FILE "rrt_goal_thread_data"

enum key_code {NONE, MOVEPOS, MOVETAN, ROTATETAN, MOVEPOSSTART, MOVETANSTART, ROTATETANSTART};


float lastx_L=0;
float lasty_L=0;
float lastx_R=0;
float lasty_R=0;



float rotate_frame[2];

float move_end[2];
float tangent_end[2];
float tangent_rotation_end[2];

float move_start[2];
float tangent_start[2];
float tangent_rotation_start[2];

GLThread* glThreads[8];
int totalThreads = 8;
int curThread = 2;
int startThread = 0;
int planThread = 2;
int realThread = 2;
int endThread = 3;
int newRRTNodeThread = 6;

// double radii[NUM_PTS];
int pressed_mouse_button;

bool drawTree;
key_code key_pressed;


Thread_RRT planner;
vector<Frame_Motion> movements;
Trajectory_Follower* follower = NULL;

int curmotion = 0;
bool initialized = false;
RRTNode* curNode;

RRTNode* localCurNode;

bool interruptEnabled = false; 


/* set up a light */
GLfloat lightOnePosition[] = {140.0, 0.0, 200.0, 0.0};
GLfloat lightOneColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightTwoPosition[] = {-140.0, 0.0, 200.0, 0.0};
GLfloat lightTwoColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightThreePosition[] = {140.0, 0.0, -200.0, 0.0};
GLfloat lightThreeColor[] = {0.99, 0.99, 0.99, 1.0};

GLfloat lightFourPosition[] = {-140.0, 0.0, -200.0, 0.0};
GLfloat lightFourColor[] = {0.99, 0.99, 0.99, 1.0};


void writeGoalThreadToFile() { 
  Trajectory_Recorder r(RRT_GOAL_THREAD_FILE);
  r.add_thread_to_list(*(glThreads[endThread]->getThread()));
  r.write_threads_to_file();

}

void readGoalThreadFromFile() { 
  Trajectory_Reader r(RRT_GOAL_THREAD_FILE);
  r.read_threads_from_file();
  vector<Thread> threads = r.get_all_threads();
  if (threads.size() > 0) { 

    Thread* frontThread = new Thread(threads.front()); 
    glThreads[endThread]->setThread(frontThread);
    glThreads[endThread]->minimize_energy();
    glutPostRedisplay();
  } 
}

void reduceDimension() {
  glThreads[curThread]->setThread(planner.halfDimApproximation(
        (const Thread*) glThreads[curThread]->getThread()));

  glThreads[curThread]->updateThreadPoints();
  glutPostRedisplay();
}

void increaseDimension() { 
  
  glThreads[curThread]->setThread(planner.doubleDimApproximation(
        (const Thread*) glThreads[curThread]->getThread()));

  glThreads[curThread]->updateThreadPoints();
  glutPostRedisplay();

}

void DimensionReductionBestPath() {

  if(!initialized) { 
    Thread* start = glThreads[planThread]->getThread();
    Thread* end = glThreads[endThread]->getThread();

    start->minimize_energy(20000, 1e-6, 0.2, 1e-7);
    end->minimize_energy(20000, 1e-6, 0.2, 1e-7);

    Thread* startApprox = planner.halfDimApproximation(start);
    Thread* endApprox = planner.halfDimApproximation(end);

    planner.initialize(startApprox, endApprox);
    glThreads[4]->setThread(new Thread(*startApprox));
    glThreads[4]->updateThreadPoints();
    glThreads[5]->setThread(new Thread(*endApprox));
    glThreads[5]->updateThreadPoints();
    glThreads[6]->setThread(new Thread(*endApprox));
    glThreads[6]->updateThreadPoints();

    initialized = true; 
  }

  
  Thread goal_thread; Thread prev_thread; Thread next_thread; 
  while(!interruptEnabled) {
  #pragma omp parallel for num_threads(12)
    for (int i = 0; i < 500; i++) {
      if (!interruptEnabled) {  
        planner.planStep(goal_thread, prev_thread, next_thread);
      }
    }
  }

  planner.updateBestPath();
  RRTNode* node = planner.getTree()->front();
  node = node->next; 
  vector<Thread*> path; 
  vector<vector<Two_Motions*> > motions; 
  while (node != NULL) {
    path.push_back(planner.doubleDimApproximation(node->thread));
    motions.push_back(node->lstMotions);
    node = node->next;
    
  }

  Thread* followerThread = new Thread(*glThreads[planThread]->getThread());
  followerThread->minimize_energy();
  follower = new Trajectory_Follower(path, motions, followerThread);

  Thread* localFollowerThread = new Thread(*glThreads[planThread]->getThread());
  localFollowerThread->minimize_energy();
  Trajectory_Follower* localFollower = new Trajectory_Follower(path, motions, localFollowerThread);

  localCurNode = new RRTNode(new Thread(*localFollowerThread));
  RRTNode* prevNode = localCurNode;
  while (!localFollower->is_done()) {
    cout << "precomputing step" << endl;
    localFollower->Take_Step(2);
    cout << "step complete" << endl; 
    RRTNode* node = new RRTNode(new Thread(*localFollower->curr_state()));
    node->prev = prevNode;
    prevNode->next = node;
    prevNode = node; 
  }

  glThreads[startThread]->setThread(new Thread(*followerThread));
  glThreads[startThread]->updateThreadPoints();

  curNode = planner.getTree()->front();
  Thread* curNodeThread = new Thread(*(curNode->thread));
  glThreads[7]->setThread(curNodeThread);
  glThreads[7]->updateThreadPoints();
  glutPostRedisplay();
}

void stepTrajectoryFollower() { 
  if (follower == NULL) return; 
  if (follower->is_done()) {
    cout << "trajectory follower done" << endl; 
    cout << "attempting solveLinearizedControl" << endl;
    if (drand48() < 0.5) { 
      solveLinearizedControl(glThreads[startThread]->getThread(), 
          glThreads[endThread]->getThread(), 
          START);
    } else {
      solveLinearizedControl(glThreads[startThread]->getThread(), 
          glThreads[endThread]->getThread(), 
          END);
    }
  } else { 
    cout << "taking step" << endl; 
    follower->Take_Step(1);
    cout << "done taking step" << endl;
    glThreads[startThread]->setThread(new Thread(*follower->curr_state()));
    glThreads[startThread]->updateThreadPoints();
  }


  glutPostRedisplay();

}

void stepTrajectoryFollower(bool forward) { 
  if(initialized) {
    if (!forward) {
      if (localCurNode->prev != NULL) {
        localCurNode = localCurNode->prev;
        Thread* localCurNodeThread = new Thread(*(localCurNode->thread));
        glThreads[startThread]->setThread(localCurNodeThread);
        glThreads[startThread]->updateThreadPoints();
        //glThreads[startThread]->minimize_energy();
      }
    }
    else { 
      if (localCurNode->next != NULL) {
        localCurNode = localCurNode->next;
        Thread* localCurNodeThread = new Thread(*(localCurNode->thread));
        glThreads[startThread]->setThread(localCurNodeThread);
        glThreads[startThread]->updateThreadPoints();
        //glThreads[startThread]->minimize_energy();
      } else { 

        cout << "trajectory follower done" << endl; 
        cout << "attempting solveLinearizedControl" << endl; 
        solveLinearizedControl(glThreads[startThread]->getThread(), 
            glThreads[endThread]->getThread(), 
            START_AND_END);

        RRTNode* node = new RRTNode(new Thread(*glThreads[startThread]->getThread()));
        node->prev = localCurNode;
        localCurNode->next = node;
        localCurNode = node; 
        glThreads[startThread]->setThread(new Thread(*node->thread));
        glThreads[startThread]->updateThreadPoints();
        //glThreads[startThread]->minimize_energy();

      }
    }
  }
  glutPostRedisplay();
}



// change prototype to include the return
void generateRandomThread() {
  glThreads[curThread]->setThread(planner.generateSample(
        (const Thread*) glThreads[curThread]->getThread()));
  
  // minimize the energy on it
  glThreads[curThread]->minimize_energy();
  
  glThreads[curThread]->updateThreadPoints();
  glutPostRedisplay();
}


void planRRT() {
  if (!initialized) {
    Thread* start = glThreads[planThread]->getThread();
    Thread* end = glThreads[endThread]->getThread();

    planner.initialize(start, end);
    initialized = true;
  }
  //VectorXd goal_thread; VectorXd prev_thread; VectorXd next_thread;
  //planner.planStep(&goal_thread, &prev_thread, &next_thread);
  Thread goal_thread; Thread prev_thread; Thread next_thread; 
  planner.planStep(goal_thread, prev_thread, next_thread);
  planner.updateBestPath(); 
  //planner.planStep(&goal_thread, &prev_thread, &next_thread);

  curNode = planner.getTree()->front();

  // draw the goal thread, the previous closest, and the new added thread
  glThreads[4]->setThread(new Thread(goal_thread));
  glThreads[5]->setThread(new Thread(prev_thread));
  glThreads[6]->setThread(new Thread(next_thread));
  //glThreads[4]->minimize_energy();
  //glThreads[5]->minimize_energy();
  //glThreads[6]->minimize_energy();
  
  if (initialized) {
    Thread* curNodeThread = new Thread(*(curNode->thread));
    glThreads[7]->setThread(curNodeThread);
    //glThreads[7]->minimize_energy();
  }
  glutPostRedisplay();
}

void stepRRT(int times) { 
  planRRT(); 
  Thread goal_thread; Thread prev_thread; Thread next_thread;

#pragma omp parallel for num_threads(12)
  for (int i = 0; (i < 500*times - 2); i++) {
    if (!interruptEnabled) {
      planner.planStep(goal_thread, prev_thread, next_thread); 
    }
  }
  planRRT(); //  this will call updateBestPath
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
  } else if (key_pressed == MOVEPOSSTART)
  {
    move_start[0] += (x-lastx_L)*MOVE_POS_CONST;
    move_start[1] += (lasty_L-y)*MOVE_POS_CONST;
  } else if (key_pressed == MOVETANSTART)
  {
    tangent_start[0] += (x-lastx_L)*MOVE_TAN_CONST;
    tangent_start[1] += (lasty_L-y)*MOVE_TAN_CONST;
  } else if (key_pressed == ROTATETANSTART)
  {
    tangent_rotation_start[0] += (x-lastx_L)*ROTATE_TAN_CONST;
    tangent_rotation_start[1] += (lasty_L-y)*ROTATE_TAN_CONST;
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
  } else if (key_pressed == MOVEPOSSTART)
  {
    move_start[0] += (x-lastx_L)*MOVE_POS_CONST;
    move_start[1] += (lasty_L-y)*MOVE_POS_CONST;
  } else if (key_pressed == MOVETANSTART)
  {
    tangent_start[0] += (x-lastx_L)*MOVE_TAN_CONST;
    tangent_start[1] += (lasty_L-y)*MOVE_TAN_CONST;
  } else if (key_pressed == ROTATETANSTART)
  {
    tangent_rotation_start[0] += (x-lastx_L)*ROTATE_TAN_CONST;
    tangent_rotation_start[1] += (lasty_L-y)*ROTATE_TAN_CONST;
  }   else {
    rotate_frame[0] += x-lastx_L;
    rotate_frame[1] += lasty_L-y;
  }

  lastx_L = x;
  lasty_L = y;

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
       Thread* curNodeThread = new Thread(*(curNode->thread));
       glThreads[7]->setThread(curNodeThread);
       //glThreads[7]->minimize_energy();
       glutPostRedisplay();
     }
   }
 } else if (key == GLUT_KEY_RIGHT) {
   if(initialized) {
     if (curNode->next != NULL) {
       curNode = curNode->next;
       Thread* curNodeThread = new Thread(*(curNode->thread));
       glThreads[7]->setThread(curNodeThread);
       //glThreads[7]->minimize_energy();
       glutPostRedisplay();
     }
   }
 }
}

void processNormalKeys(unsigned char key, int x, int y)
{
  interruptEnabled = false; 
  if (key == 't') {
    key_pressed = MOVETAN;
  } else if (key == 'm') {
    key_pressed = MOVEPOS;
  } else if (key == 'r') {
    key_pressed = ROTATETAN;
  } else if (key == 'T') {
    key_pressed = MOVETANSTART;
  } else if (key == 'M') {
    key_pressed = MOVEPOSSTART;
  } else if (key == 'R') {
    key_pressed = ROTATETANSTART;
  } else if (key == 'l') {
    solveLinearizedControl(glThreads[planThread]->getThread(), 
                           glThreads[endThread]->getThread(), 
                           START_AND_END);
    glThreads[planThread]->updateThreadPoints();
    glutPostRedisplay();
  }
  else if (key == 'p') {
    planMovement();
  }
  else if (key == 'd') {
    generateRandomThread();
  } 
  else if (key == 'a') {
    planRRT();
  } 
  else if (key == 's') { 
    stepRRT(100); 
  }
  else if (key == 'x') {
    writeGoalThreadToFile();
  }
  else if (key == '/') {
    readGoalThreadFromFile();
  }
  else if (key == 'v') { 
    reduceDimension();
  }
  else if (key == 'b') { 
    increaseDimension();
  }
  else if (key == 'n') {
    DimensionReductionBestPath();
  }
  else if (key == '>') {
    stepTrajectoryFollower(true); 
  } 
  else if (key == '<') {
    stepTrajectoryFollower(false); 
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
  } else if (key == '-' || key == '_' ) {
    selectThread(-1);
  } else if (key == 'w') {
    drawTree = !drawTree;
    glutPostRedisplay ();
  }
  key_pressed = NONE;
  move_end[0] = move_end[1] = tangent_end[0] = tangent_end[1] = tangent_rotation_end[0] = tangent_rotation_end[1] = 0.0;
}

void interruptHandler(int sig) {
  cout << "Signal " << sig << " caught..." << endl;
  interruptEnabled = true; 
}



void JoinStyle (int msg)
{
  exit (0);
}


int main (int argc, char * argv[])
{

  //srand(time(NULL));
  //srand((unsigned int)time((time_t *)NULL));


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


  signal(SIGINT, &interruptHandler);

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


 if (move_end[0] != 0.0 || move_end[1] != 0.0 || tangent_end[0] != 0.0 || tangent_end[1] != 0.0 || tangent_rotation_end[0] != 0 || tangent_rotation_end[1] != 0 || move_start[0] != 0.0 || move_start[1] != 0.0 || tangent_start[0] != 0.0 || tangent_start[1] != 0.0 || tangent_rotation_start[0] != 0 || tangent_rotation_start[1] != 0)
  {
    glThreads[curThread]->ApplyUserInput(move_end, tangent_end, tangent_rotation_end, move_start, tangent_start, tangent_rotation_start);
  }

  // draw planner end points
  if(drawTree) {
    DrawRRT();
  }


  //Draw Axes
  DrawAxes();



  for(int i = 0; i < totalThreads; i++) {
    if ( !initialized && i != planThread  && i != endThread ) continue; 
    if ( initialized && i!=startThread && i != planThread && i != endThread 
        && i != 4 && i != 5 && i != 6 && i != 7 ) continue; 

    glThreads[i]->DrawAxes();

    //Draw Thread
    if (i==curThread) {
      glColor3f (0.8, 0.4, 0.0);
    } else if (i==planThread) {
      glColor3f (0.2, 0.8, 0.2);
    } else if (i==startThread) {
      glColor3f (0.25, 0.55, 1.0); 
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
