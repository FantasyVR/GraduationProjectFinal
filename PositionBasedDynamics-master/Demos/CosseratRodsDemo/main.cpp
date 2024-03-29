#include "Common/Common.h"
#include "Demos/Visualization/MiniGL.h"
#include "Demos/Visualization/Selection.h"
#include "GL/glut.h"
#include "Simulation/TimeManager.h"
#include <Eigen/Dense>
#include "Simulation/SimulationModel.h"
#include "Simulation/TimeStepController.h"
#include <iostream>
#include "Demos/Visualization/Visualization.h"
#include "Utils/Logger.h"
#include "Utils/Timing.h"
#include "Utils/FileSystem.h"
#include "Demos/Common/DemoBase.h"
#include "Demos/Common/TweakBarParameters.h"
#include "Simulation/Simulation.h"
#include "Simulation/DistanceFieldCollisionDetection.h"
#include <time.h>
// Enable memory leak detection
#if defined(_DEBUG) && !defined(EIGEN_ALIGN)
	#define new DEBUG_NEW 
#endif
int Pointlast,pl0;
using namespace PBD;
using namespace Eigen;
using namespace std;
using namespace Utilities;

void initParameters();
void timeStep ();
void buildModel ();
void createHelix(const Vector3r &position, const Matrix3r &orientation, Real radius, Real height, Real totalAngle, int nPoints);
void render ();
void reset();
void func_b();
void func_n();
void func_1();
void func_2();
void func_3();
void func_4();
void func_5();
void func_6();
void func_o();
void func_p();
void TW_CALL setStretchingStiffness(const void *value, void *clientData);
void TW_CALL getStretchingStiffness(void *value, void *clientData);
void TW_CALL setShearingStiffness1(const void *value, void *clientData);
void TW_CALL getShearingStiffness1(void *value, void *clientData);
void TW_CALL setShearingStiffness2(const void *value, void *clientData);
void TW_CALL getShearingStiffness2(void *value, void *clientData);
void TW_CALL setBendingStiffness1(const void *value, void *clientData);
void TW_CALL getBendingStiffness1(void *value, void *clientData);
void TW_CALL setBendingStiffness2(const void *value, void *clientData);
void TW_CALL getBendingStiffness2(void *value, void *clientData);
void TW_CALL setTwistingStiffness(const void *value, void *clientData);
void TW_CALL getTwistingStiffness(void *value, void *clientData);

DemoBase *base;

const unsigned int nParticles = 50;
const Real helixRadius = 0.5;
const Real helixHeight = -5.0;
const Real helixTotalAngle = static_cast<Real>(10.0*M_PI);
const Matrix3r helixOrientation = AngleAxisr(-static_cast<Real>(0.5 * M_PI), Vector3r(0,1,0)).toRotationMatrix();
bool drawFrames = false;

DistanceFieldCollisionDetection cd;

// main 
int main( int argc, char **argv )
{
	REPORT_MEMORY_LEAKS

	base = new DemoBase();
	base->init(argc, argv, "Cosserat rods demo");

	SimulationModel *model = new SimulationModel();
	model->init();
	Simulation::getCurrent()->setModel(model);

	buildModel();

	initParameters();

	Simulation::getCurrent()->setSimulationMethodChangedCallback([&]() { reset(); initParameters(); base->getSceneLoader()->readParameterObject(Simulation::getCurrent()->getTimeStep()); });

	// OpenGL
	MiniGL::setClientIdleFunc (50, timeStep);		
	MiniGL::setKeyFunc(0, 'r', reset);
	MiniGL::setKeyFunc(1, 'b', func_b);
	MiniGL::setKeyFunc(2, 'n', func_n);

	MiniGL::setKeyFunc(3, '7', func_1);
	MiniGL::setKeyFunc(4, '8', func_2);
	MiniGL::setKeyFunc(5, 'i', func_3);
	MiniGL::setKeyFunc(6, 'u', func_4);
	MiniGL::setKeyFunc(7, '5', func_5);
	MiniGL::setKeyFunc(8, '6', func_6);

	MiniGL::setKeyFunc(9, 'o', func_o);
	MiniGL::setKeyFunc(10, 'p', func_p);

	MiniGL::setClientSceneFunc(render);			
	MiniGL::setViewport (40.0f, 0.1f, 500.0f, Vector3r (5.0, 10.0, 30.0), Vector3r (5.0, 0.0, 0.0));

	TwAddVarRW(MiniGL::getTweakBar(), "Draw frames", TW_TYPE_BOOLCPP, &drawFrames, " label='Draw frames' group=Simulation");
	TwAddVarCB(MiniGL::getTweakBar(), "Stretching stiffness", TW_TYPE_REAL, setStretchingStiffness, getStretchingStiffness, model, " label='Stretching stiffness'  min=0.0 max=1.0 step=0.1 precision=4 group='Stretch shear constraints' ");
	TwAddVarCB(MiniGL::getTweakBar(), "Shearing stiffness 1", TW_TYPE_REAL, setShearingStiffness1, getShearingStiffness1, model, " label='Shearing stiffness 1'  min=0.0 max=1.0 step=0.1 precision=4 group='Stretch shear constraints' ");
	TwAddVarCB(MiniGL::getTweakBar(), "Shearing stiffness 2", TW_TYPE_REAL, setShearingStiffness2, getShearingStiffness2, model, " label='Shearing stiffness 2'  min=0.0 max=1.0 step=0.1 precision=4 group='Stretch shear constraints' ");
	TwAddVarCB(MiniGL::getTweakBar(), "Bending stiffness 1", TW_TYPE_REAL, setBendingStiffness1, getBendingStiffness1, model, " label='Bending stiffness 1'  min=0.0 max=1.0 step=0.1 precision=4 group='Bend twist constraints' ");
	TwAddVarCB(MiniGL::getTweakBar(), "Bending stiffness 2", TW_TYPE_REAL, setBendingStiffness2, getBendingStiffness2, model, " label='Bending stiffness 2'  min=0.0 max=1.0 step=0.1 precision=4 group='Bend twist constraints' ");
	TwAddVarCB(MiniGL::getTweakBar(), "Twisting stiffness", TW_TYPE_REAL, setTwistingStiffness, getTwistingStiffness, model, " label='Twisting stiffness'  min=0.0 max=1.0 step=0.1 precision=4 group='Bend twist constraints' ");

	glutMainLoop ();	

	Utilities::Timing::printAverageTimes();
	Utilities::Timing::printTimeSums();

	delete Simulation::getCurrent();
	delete base;
	delete model;

	return 0;
}

void initParameters()
{
	TwRemoveAllVars(MiniGL::getTweakBar());
	TweakBarParameters::cleanup();

	MiniGL::initTweakBarParameters();

	TweakBarParameters::createParameterGUI();
	TweakBarParameters::createParameterObjectGUI(base);
	TweakBarParameters::createParameterObjectGUI(Simulation::getCurrent());
	TweakBarParameters::createParameterObjectGUI(Simulation::getCurrent()->getModel());
	TweakBarParameters::createParameterObjectGUI(Simulation::getCurrent()->getTimeStep());
}

void reset()
{
	Utilities::Timing::printAverageTimes();
	Utilities::Timing::reset();

	Simulation::getCurrent()->reset();
	base->getSelectedParticles().clear();

	Simulation::getCurrent()->getModel()->cleanup();
	buildModel();
}
void func_o()
{
	Pointlast++;
	Pointlast = min(Pointlast, pl0);
}
void func_p()
{
	Pointlast--;
	Pointlast = max(Pointlast, 0);
}
void func_1()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	x += (helixRadius / 3);
	Vector3r newpos = Vector3r(x,y,z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_2()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	x -= (helixRadius / 3);
	Vector3r newpos = Vector3r(x, y, z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_3()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	y += (helixRadius / 3);
	Vector3r newpos = Vector3r(x, y, z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_4()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	y -= (helixRadius / 3);
	Vector3r newpos = Vector3r(x, y, z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_5()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	z += (helixRadius / 3);
	Vector3r newpos = Vector3r(x, y, z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_6()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	ParticleData &pd = model->getParticles();
	const unsigned int nParticles = pd.getNumberOfParticles();
	Vector3r pos = pd.getPosition(Pointlast);
	//int c = helixRadius;
	double x = pos(0, 0);
	double y = pos(1, 0);
	double z = pos(2, 0);
	z -= (helixRadius / 3);
	Vector3r newpos = Vector3r(x, y, z);
	pd.setPosition(Pointlast, newpos);
	pd.setLastPosition(Pointlast, newpos);
	pd.setOldPosition(Pointlast, newpos);
}
void func_b()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	OrientationData &od = model->getOrientations();
	const unsigned int nQuaternionsTotal = od.getNumberOfQuaternions();
	Quaternionr dq = od.getQuaternion(nQuaternionsTotal - 1);
	
	AngleAxisr angleAxis(20, Vector3r(0, 0, 1));
	Quaternionr quat(angleAxis);
	dq *= quat;
	od.setQuaternion(nQuaternionsTotal - 1, dq);
	od.setLastQuaternion(nQuaternionsTotal - 1, dq);
	od.setOldQuaternion(nQuaternionsTotal - 1, dq);
}
void func_n()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	OrientationData &od = model->getOrientations();
	const unsigned int nQuaternionsTotal = od.getNumberOfQuaternions();
	Quaternionr dq = od.getQuaternion(0);

	AngleAxisr angleAxis(20, Vector3r(0, 0, -1));
	Quaternionr quat(angleAxis);
	//m_rotation = m_rotation * quat;
	//Quaternionr dqd1;
	//dqd1 = Quaternionr::FromTwoVectors(from, ttt);
	dq *= quat;
	od.setQuaternion(0, dq);
	od.setLastQuaternion(0, dq);
	od.setOldQuaternion(0, dq);
}


void timeStep ()
{
	const Real pauseAt = base->getValue<Real>(DemoBase::PAUSE_AT);
	if ((pauseAt > 0.0) && (pauseAt < TimeManager::getCurrent()->getTime()))
		base->setValue(DemoBase::PAUSE, true);

	if (base->getValue<bool>(DemoBase::PAUSE))
		return;

	// Simulation code
	SimulationModel *model = Simulation::getCurrent()->getModel();
	const unsigned int numSteps = base->getValue<unsigned int>(DemoBase::NUM_STEPS_PER_RENDER);
	for (unsigned int i = 0; i < numSteps; i++)
	{
		START_TIMING("SimStep");
		Simulation::getCurrent()->getTimeStep()->step(*model);
		//OrientationData &od = model->getOrientations();
		//const unsigned int nQuaternionsTotal = od.getNumberOfQuaternions();
		//Quaternionr dq = od[nQuaternionsTotal - 1];
		//Quaternionr dq = od.getLastQuaternion(nQuaternionsTotal - 1);
		//Vector3r from(0, 1, 0), ttt(1, 0, 0);
		//Quaternionr dqd1;
		//dqd1 = Quaternionr::FromTwoVectors(from, ttt);
		//dq *= dqd1;
		//od.setLastQuaternion(nQuaternionsTotal - 1, dq);

		//model.getQ
		STOP_TIMING_AVG;
	}
}

void buildModel ()
{
	TimeManager::getCurrent ()->setTimeStepSize (static_cast<Real>(0.005));
	
	createHelix(Vector3r(0, 0, 0), helixOrientation, helixRadius, helixHeight, helixTotalAngle, nParticles);
}

void renderLineModels()
{
	// Draw simulation model
	SimulationModel *model = Simulation::getCurrent()->getModel();
	const ParticleData &pd = model->getParticles();
	const OrientationData &od = model->getOrientations();
	float red[4] = { 0.8f, 0.0f, 0.0f, 1 };
	float green[4] = { 0.0f, 0.8f, 0.0f, 1 };
	float blue[4] = { 0.0f, 0.0f, 0.8f, 1 };

	for (unsigned int i = 0; i < model->getLineModels().size(); i++)
	{
		LineModel *lineModel = model->getLineModels()[i];

		for(unsigned int e=0; e<lineModel->getEdges().size(); e++)
		{
			const unsigned int indexOffset = lineModel->getIndexOffset();
			const unsigned int indexOffsetQuaternions = lineModel->getIndexOffsetQuaternions();
			const unsigned int i1 = lineModel->getEdges()[e].m_vert[0] + indexOffset;
			const unsigned int i2 = lineModel->getEdges()[e].m_vert[1] + indexOffset;
			const unsigned int iq = lineModel->getEdges()[e].m_quat + indexOffsetQuaternions;
			const Vector3r &v1 = pd.getPosition(i1);
			const Vector3r &v2 = pd.getPosition(i2);
			const Quaternionr &q = od.getQuaternion(iq);
			
			MiniGL::drawSphere(v1, 0.07f, blue);
			if( e == lineModel->getEdges().size() -1 ) MiniGL::drawSphere(v2, 0.07f, blue);
			if(drawFrames) MiniGL::drawCylinder(v1, v2, blue, 0.01f);
			else MiniGL::drawCylinder(v1, v2, blue, 0.07f);

			//draw coordinate frame at the center of the edges
			if(drawFrames)
			{
				Vector3r vm = 0.5 * (v1 + v2);
				Real scale = static_cast<Real>(0.15);
				Vector3r d1 = q._transformVector(Vector3r(1, 0, 0)) * scale;
				Vector3r d2 = q._transformVector(Vector3r(0, 1, 0)) * scale;
				Vector3r d3 = q._transformVector(Vector3r(0, 0, 1)) * scale;
				MiniGL::drawCylinder(vm, vm + d1, red,   0.01f);
				MiniGL::drawCylinder(vm, vm + d2, green, 0.01f);
				MiniGL::drawCylinder(vm, vm + d3, blue,  0.01f);
			}
		}		
	}
}

void render ()
{
	base->render();

	renderLineModels();
}

/** Create a particle model and orientation model
*/
void createHelix(const Vector3r &position, const Matrix3r &orientation, Real radius, Real height, Real totalAngle, int nPoints)
{
	
	//nPoints *= 2;
	//nPoints /= 3;
	int nQuaternions = nPoints - 1;
	vector<Vector3r> points(nPoints);
	vector<Quaternionr> quaternions(nQuaternions);
	//totalAngle /= 2;
	//init particles
	for (int i = 0; i<nPoints; i++)   
	{
	//	points[i].x() = radius * std::cos(totalAngle / ((Real)nPoints) * (Real)i);
	//	points[i].y() = radius * std::sin(totalAngle / ((Real)nPoints) * (Real)i);
		points[i].z() = height / ((Real)nPoints) * (Real)i;
		points[i].x() = 0;
		points[i].y() = height * std::sin(3.1415 / ((Real)nPoints) * (Real)i) * 2;
		points[i] = orientation * points[i] + position;
	}

	//init quaternions
	Vector3r from(0, 0, 1),ttt(1, 1, 0);
	Quaternionr dqd1;
	for(int i=0; i<nQuaternions ; i++)	
	{
		Vector3r to = (points[i + 1] - points[i]).normalized();
		Quaternionr dq = Quaternionr::FromTwoVectors(from, to);
		if(i == 0) quaternions[i] = dq;
		else quaternions[i] = dq * quaternions[i - 1];
		from = to;
		
	}
	dqd1 = Quaternionr::FromTwoVectors(from, ttt);
///////////////////////////////////
//	for (int i = 0; i < 1; i++)
//	{
//		quaternions[nQuaternions - 1]*= dqd1;
//	}


	vector<unsigned int> indices(2 * nPoints - 1);
	vector<unsigned int> indicesQuaternions(nQuaternions);

	for(int i=0; i < nPoints-1 ; i++)
	{
		indices[2 * i] = i;
		indices[2 * i + 1] = i + 1;
	}

	for (int i = 0; i < nQuaternions; i++)
	{
		indicesQuaternions[i] = i;
	}


	SimulationModel *model = Simulation::getCurrent()->getModel();
	Simulation::getCurrent()->getTimeStep()->setCollisionDetection(*model, &cd);
	cd.setTolerance(static_cast<Real>(-0.00003));

	model->addLineModel(nPoints, nQuaternions, &points[0], &quaternions[0], &indices[0], &indicesQuaternions[0]);
///////////////////////////////////////////
	//for robust
	for (int i = 1; i < nPoints - 1; i++)
	{
	//	points[i].x() = 10 * (float)(rand() % nPoints) / (float)nPoints;//radius * std::cos(totalAngle / ((Real)nPoints) * (Real)i);
	//	points[i].y() = 10 * (float)(rand() % nPoints) / (float)nPoints;//radius * std::sin(totalAngle / ((Real)nPoints) * (Real)i);
	//	points[i].z() = 10 * (float)(rand() % nPoints) / (float)nPoints;//height / ((Real)nPoints) * (Real)i;

	//	points[i] = orientation * points[i] + position;
	}
	
	model->iniLineModel(nPoints,&points[0]);
	ParticleData &pd = model->getParticles();
	const int nPointsTotal = pd.getNumberOfParticles();
	for (int i = nPointsTotal - 2; i > nPointsTotal - nPoints; i--)
	{
		pd.setMass(i, 1.0);
	}
	pd.setMass(nPointsTotal -1, 0.00);
	Pointlast = nPointsTotal - 1;
	pl0 = Pointlast;
	//pd.setMass(nPointsTotal - 2, 0.0);
	// Set mass of points to zero => make it static
	pd.setMass(nPointsTotal - nPoints , 0.0);

	OrientationData &od = model->getOrientations();
	const unsigned int nQuaternionsTotal = od.getNumberOfQuaternions();
	for(unsigned int i = nQuaternionsTotal - 2; i > nQuaternionsTotal - nQuaternions; i--)
	{
		od.setMass(i, 1.0);
		//od.setLastQuaternion
	}

	od.setMass(nQuaternionsTotal - 1, 0.0);
	// Set mass of quaternions to zero => make it static
	od.setMass(nQuaternionsTotal - nQuaternions, 0.0);

	// init constraints
	const size_t rodNumber = model->getLineModels().size() - 1;
	const unsigned int offset = model->getLineModels()[rodNumber]->getIndexOffset();
	const unsigned int offsetQuaternions = model->getLineModels()[rodNumber]->getIndexOffsetQuaternions();
	const size_t nEdges = model->getLineModels()[rodNumber]->getEdges().size();
	const LineModel::Edges &edges = model->getLineModels()[rodNumber]->getEdges();
		
	//stretchShear constraints
	for(unsigned int i=0; i < nEdges; i++)
	{
		const unsigned int v1 = edges[i].m_vert[0] + offset;
		const unsigned int v2 = edges[i].m_vert[1] + offset;
		const unsigned int q1 = edges[i].m_quat + offsetQuaternions;
		model->addStretchShearConstraint(v1, v2, q1);
	}

	//bendTwist constraints
	for(unsigned int i=0; i < nEdges - 1; i++)
	{
		const unsigned int q1 = edges[i].m_quat + offsetQuaternions;
		const unsigned int q2 = edges[i + 1].m_quat + offsetQuaternions;
		model->addBendTwistConstraint(q1, q2);
	}
	for(unsigned int i = 0;i < nEdges ;i ++)
		for(unsigned int j = i + 1;j < nEdges; j ++)
		//if(i != j)
		{
			const unsigned int v1 = edges[i].m_vert[0] + offset;
			const unsigned int v2 = edges[i].m_vert[1] + offset;
			const unsigned int v3 = edges[j].m_vert[0] + offset;
			const unsigned int v4 = edges[j].m_vert[1] + offset;
			model->addLineLineConstraint(v1, v2, v3, v4);
		}
	
	
// 	LOG_INFO << "Number of particles: " << nPoints;
// 	LOG_INFO << "Number of quaternions: " << nQuaternions;
}

void TW_CALL setStretchingStiffness(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodStretchingStiffness(val);
}

void TW_CALL getStretchingStiffness(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodStretchingStiffness();
}

void TW_CALL setShearingStiffness1(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodShearingStiffness1(val);
}

void TW_CALL getShearingStiffness1(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodShearingStiffness1();
}

void TW_CALL setShearingStiffness2(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodShearingStiffness2(val);
}

void TW_CALL getShearingStiffness2(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodShearingStiffness2();
}

void TW_CALL setBendingStiffness1(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodBendingStiffness1(val);
}

void TW_CALL getBendingStiffness1(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodBendingStiffness1();
}

void TW_CALL setBendingStiffness2(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodBendingStiffness2(val);
}

void TW_CALL getBendingStiffness2(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodBendingStiffness2();
}

void TW_CALL setTwistingStiffness(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setRodTwistingStiffness(val);
}

void TW_CALL getTwistingStiffness(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getRodTwistingStiffness();
}