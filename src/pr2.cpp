#include "pr2.h"

static const char LEFT_GRIPPER_LEFT_FINGER_NAME[] = "l_gripper_l_finger_tip_link";
static const char LEFT_GRIPPER_RIGHT_FINGER_NAME[] = "l_gripper_r_finger_tip_link";

static const char RIGHT_GRIPPER_LEFT_FINGER_NAME[] = "r_gripper_l_finger_tip_link";
static const char RIGHT_GRIPPER_RIGHT_FINGER_NAME[] = "r_gripper_r_finger_tip_link";

// adapted from btSoftBody.cpp (btSoftBody::appendAnchor)
static void btSoftBody_appendAnchor(btSoftBody *psb, btSoftBody::Node *node, btRigidBody *body, btScalar influence=1) {
    btSoftBody::Anchor a;
    a.m_node = node;
    a.m_body = body;
    a.m_local = body->getWorldTransform().inverse()*a.m_node->m_x;
    a.m_node->m_battach = 1;
    a.m_influence = influence;
    psb->m_anchors.push_back(a);
}

// Fills in the rcontacs array with contact information between psb and pco
static void getContactPointsWith(btSoftBody *psb, btCollisionObject *pco, btSoftBody::tRContactArray &rcontacts) {
    // custom contact checking adapted from btSoftBody.cpp and btSoftBodyInternals.h
    struct Custom_CollideSDF_RS : btDbvt::ICollide {
        Custom_CollideSDF_RS(btSoftBody::tRContactArray &rcontacts_) : rcontacts(rcontacts_) { }

        void Process(const btDbvtNode* leaf) {
            btSoftBody::Node* node=(btSoftBody::Node*)leaf->data;
            DoNode(*node);
        }

        void DoNode(btSoftBody::Node& n) {
            const btScalar m=n.m_im>0?dynmargin:stamargin;
            btSoftBody::RContact c;
            if (!n.m_battach && psb->checkContact(m_colObj1,n.m_x,m,c.m_cti)) {
                const btScalar  ima=n.m_im;
                const btScalar  imb= m_rigidBody? m_rigidBody->getInvMass() : 0.f;
                const btScalar  ms=ima+imb;
                if(ms>0) {
                    // there's a lot of extra information we don't need to compute
                    // since we just want to find the contact points
#if 0
                    const btTransform&      wtr=m_rigidBody?m_rigidBody->getWorldTransform() : m_colObj1->getWorldTransform();
                    static const btMatrix3x3        iwiStatic(0,0,0,0,0,0,0,0,0);
                    const btMatrix3x3&      iwi=m_rigidBody?m_rigidBody->getInvInertiaTensorWorld() : iwiStatic;
                    const btVector3         ra=n.m_x-wtr.getOrigin();
                    const btVector3         va=m_rigidBody ? m_rigidBody->getVelocityInLocalPoint(ra)*psb->m_sst.sdt : btVector3(0,0,0);
                    const btVector3         vb=n.m_x-n.m_q; 
                    const btVector3         vr=vb-va;
                    const btScalar          dn=btDot(vr,c.m_cti.m_normal);
                    const btVector3         fv=vr-c.m_cti.m_normal*dn;
                    const btScalar          fc=psb->m_cfg.kDF*m_colObj1->getFriction();
#endif
                    c.m_node        =       &n;
#if 0
                    c.m_c0          =       ImpulseMatrix(psb->m_sst.sdt,ima,imb,iwi,ra);
                    c.m_c1          =       ra;
                    c.m_c2          =       ima*psb->m_sst.sdt;
                    c.m_c3          =       fv.length2()<(btFabs(dn)*fc)?0:1-fc;
                    c.m_c4          =       m_colObj1->isStaticOrKinematicObject()?psb->m_cfg.kKHR:psb->m_cfg.kCHR;
#endif
                    rcontacts.push_back(c);
#if 0
                    if (m_rigidBody)
                            m_rigidBody->activate();
#endif
                }
            }
        }
        btSoftBody*             psb;
        btCollisionObject*      m_colObj1;
        btRigidBody*    m_rigidBody;
        btScalar                dynmargin;
        btScalar                stamargin;
        btSoftBody::tRContactArray &rcontacts;
    };

    Custom_CollideSDF_RS  docollide(rcontacts);              
    btRigidBody*            prb1=btRigidBody::upcast(pco);
    btTransform     wtr=pco->getWorldTransform();

    const btTransform       ctr=pco->getWorldTransform();
    const btScalar          timemargin=(wtr.getOrigin()-ctr.getOrigin()).length();
    const btScalar          basemargin=psb->getCollisionShape()->getMargin();
    btVector3                       mins;
    btVector3                       maxs;
    ATTRIBUTE_ALIGNED16(btDbvtVolume)               volume;
    pco->getCollisionShape()->getAabb(      pco->getWorldTransform(),
            mins,
            maxs);
    volume=btDbvtVolume::FromMM(mins,maxs);
    volume.Expand(btVector3(basemargin,basemargin,basemargin));             
    docollide.psb           =       psb;
    docollide.m_colObj1 = pco;
    docollide.m_rigidBody = prb1;

    docollide.dynmargin     =       basemargin+timemargin;
    docollide.stamargin     =       basemargin;
    psb->m_ndbvt.collideTV(psb->m_ndbvt.m_root,volume,docollide);
}

PR2SoftBodyGripper::PR2SoftBodyGripper(RaveRobotKinematicObject::Manipulator::Ptr manip_, bool leftGripper) :
        manip(manip_),
        leftFinger(manip->robot->robot->GetLink(leftGripper ? LEFT_GRIPPER_LEFT_FINGER_NAME : RIGHT_GRIPPER_LEFT_FINGER_NAME)),
        rightFinger(manip->robot->robot->GetLink(leftGripper ? LEFT_GRIPPER_RIGHT_FINGER_NAME : RIGHT_GRIPPER_RIGHT_FINGER_NAME)),
        origLeftFingerInvTrans(manip->robot->getLinkTransform(leftFinger).inverse()),
        origRightFingerInvTrans(manip->robot->getLinkTransform(rightFinger).inverse()),
        centerPt(manip->getTransform().getOrigin()),
        closingNormal(manip->manip->GetClosingDirection()[0],
                      manip->manip->GetClosingDirection()[1],
                      manip->manip->GetClosingDirection()[2]),
        toolDirection(util::toBtVector(manip->manip->GetLocalToolDirection())) // don't bother scaling
{
}

void PR2SoftBodyGripper::attach(bool left) {
    btRigidBody *rigidBody =
        manip->robot->associatedObj(left ? leftFinger : rightFinger)->rigidBody.get();
    btSoftBody::tRContactArray rcontacts;
    getContactPointsWith(psb, rigidBody, rcontacts);
    cout << "got " << rcontacts.size() << " contacts\n";
    int nAppended = 0;
    for (int i = 0; i < rcontacts.size(); ++i) {
        const btSoftBody::RContact &c = rcontacts[i];
        KinBody::LinkPtr colLink = manip->robot->associatedObj(c.m_cti.m_colObj);
        if (!colLink) continue;
        const btVector3 &contactPt = c.m_node->m_x;
        if (onInnerSide(contactPt, left)) {
            btSoftBody_appendAnchor(psb, c.m_node, rigidBody);
            ++nAppended;
        }
    }
    cout << "appended " << nAppended << " anchors\n";
}
