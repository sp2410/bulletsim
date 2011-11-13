#include "rope.h"
#include <iostream>
using namespace std;

CapsuleRope::CapsuleRope(const btAlignedObjectArray<btVector3>& ctrlPoints, btScalar radius_) {
  radius = radius_;
  nLinks = ctrlPoints.size() - 1;
  bodies.resize(nLinks);
  shapes.resize(nLinks);
  joints.resize(nLinks-1);


  for (int i=0; i < nLinks; i++) {
    btVector3 pt0 = ctrlPoints[i];
    btVector3 pt1 = ctrlPoints[i+1];
    btVector3 midpt = (pt0+pt1)/2;
    btVector3 diff = (pt1-pt0);
    btQuaternion q;
    btScalar ang = diff.angle(btVector3(1,0,0));
    if (ang > 0) {
      btVector3 ax = diff.cross(btVector3(1,0,0));
      q = btQuaternion(ax,ang);
    }
    else {
      q = btQuaternion();
    }
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(midpt);
    trans.setRotation(q);

    float len = diff.length();
    float mass = 1;//mass = 3.14*radius*radius*len;
    //btCollisionShape* shape = new btCapsuleShapeX(radius,len);
    btCollisionShape* shape = new btCylinderShapeX(btVector3(len/3,radius,radius));
    shapes[i].reset(shape);
    //btCollisionShape* shape = new btSphereShape(len/2.1);

    bool isDynamic = (mass != 0.f);
    btVector3 localInertia(0,0,0);
    if (isDynamic) shape->calculateLocalInertia(mass,localInertia);
    btDefaultMotionState* myMotionState = new btDefaultMotionState(trans);
    btRigidBody::btRigidBodyConstructionInfo cInfo(mass,myMotionState,shape,localInertia);
    btRigidBody* body = new btRigidBody(cInfo);

    cout << body->getCenterOfMassPosition().x() << endl;
    bodies[i].reset(body);

    BulletObject::Ptr child(new BulletObject(shapes[i],bodies[i]));
    children.push_back(child);

    if (i>0) {
      btPoint2PointConstraint* joint = new btPoint2PointConstraint(*bodies[i-1],*bodies[i],btVector3(len/2,0,0),btVector3(-len/2,0,0));
      joints[i-1].reset(joint);
    }


  }
}

void CapsuleRope::init() {



  cout << "rope initializing" << endl;
  CompoundObject::init();



   for (int i=0; i < children.size()-1; i++) {
     //     BulletObject::Ptr bo0 = static_cast<BulletObject::Ptr>(children[i]);
     //BulletObject::Ptr bo1 = static_cast<BulletObject::Ptr>(children[i+1]);

     //btRigidBody* body0 = (bo0->rigidBody).get();
     //btRigidBody* body1 = (bo1->rigidBody).get();
     //        btPoint2PointConstraint* joint = new btPoint2PointConstraint(*body0,*body1,btVector3(.1/2,0,0),btVector3(-.1/2,0,0));
  //      //getEnvironment()->bullet->dynamicsWorld->addConstraint(joint);
   }


  for (int i=0; i< joints.size(); i++) {
    //   getEnvironment()->bullet->dynamicsWorld->addConstraint(joints[i].get(),true);
  }
}

