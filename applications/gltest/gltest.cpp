/**
 * Small application exercising the GL support of the qfreerdp QPA.
 *
 * usage: gltest -platform freerdp [options]
 *   --mode window|quick|widget   kind of GL window (default: window)
 *                                  window: QOpenGLWindow (raw GLES2)
 *                                  quick:  QQuickView (scenegraph, threaded render loop by default)
 *                                  widget: QOpenGLWidget inside a QWidget
 *                                  raster: plain QWidget, no GL (for comparison)
 *   --windows N                  number of windows (default 1)
 *   --resize                     resize/move the windows continuously
 *   --cycle MS                   destroy and recreate the windows every MS ms
 *   --duration S                 quit after S seconds and print statistics
 *
 * Each GL window draws:
 *   - an animated background
 *   - a rotating triangle
 *   - a green square in the top-left corner, a blue one in the bottom-right corner
 *     (so that flipped/shifted readbacks are easy to spot)
 */
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QOpenGLWindow>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickView>
#include <QRandomGenerator>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <cstdio>

static QAtomicInt g_frames;
static QAtomicInt g_created;
static bool g_rendererPrinted = false;

/** GLES2 renderer shared by the QOpenGLWindow and QOpenGLWidget variants */
class Renderer : protected QOpenGLFunctions {
public:
	void init() {
		initializeOpenGLFunctions();
		if (!g_rendererPrinted) {
			g_rendererPrinted = true;
			printf("GL_VENDOR=%s\nGL_RENDERER=%s\nGL_VERSION=%s\n", glGetString(GL_VENDOR),
					glGetString(GL_RENDERER), glGetString(GL_VERSION));
			fflush(stdout);
		}

		mProgram = new QOpenGLShaderProgram();
		mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
			"attribute highp vec2 pos;\n"
			"attribute lowp vec3 col;\n"
			"uniform highp float angle;\n"
			"varying lowp vec3 vcol;\n"
			"void main() {\n"
			"  float c = cos(angle), s = sin(angle);\n"
			"  gl_Position = vec4(c * pos.x - s * pos.y, s * pos.x + c * pos.y, 0.0, 1.0);\n"
			"  vcol = col;\n"
			"}\n");
		mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
			"varying lowp vec3 vcol;\n"
			"void main() { gl_FragColor = vec4(vcol, 1.0); }\n");
		mProgram->bindAttributeLocation("pos", 0);
		mProgram->bindAttributeLocation("col", 1);
		if (!mProgram->link())
			qFatal("unable to link program: %s", qPrintable(mProgram->log()));
		mTimer.start();
	}

	void cleanup() {
		delete mProgram;
		mProgram = nullptr;
	}

	void render(int w, int h) {
		const float t = mTimer.elapsed() / 1000.0f;

		glViewport(0, 0, w, h);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0.25f + 0.25f * qSin(t), 0.2f, 0.25f + 0.25f * qCos(t), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		static const GLfloat verts[] = { 0.0f, 0.7f, -0.6f, -0.5f, 0.6f, -0.5f };
		static const GLfloat cols[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		mProgram->bind();
		mProgram->setUniformValue("angle", t);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, cols);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		mProgram->release();

		/* orientation markers (GL origin is bottom-left) */
		const int sz = 30;
		glEnable(GL_SCISSOR_TEST);
		glScissor(0, h - sz, sz, sz);
		glClearColor(0, 1, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(w - sz, 0, sz, sz);
		glClearColor(0, 0, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);

		g_frames.fetchAndAddRelaxed(1);
	}

protected:
	QOpenGLShaderProgram *mProgram = nullptr;
	QElapsedTimer mTimer;
};


class GlWindow : public QOpenGLWindow {
public:
	GlWindow() {
		connect(this, &QOpenGLWindow::frameSwapped, this, QOverload<>::of(&QPaintDeviceWindow::update));
	}
	~GlWindow() {
		makeCurrent();
		mRenderer.cleanup();
		doneCurrent();
	}

protected:
	void initializeGL() override { mRenderer.init(); }
	void paintGL() override {
		const qreal dpr = devicePixelRatio();
		mRenderer.render(width() * dpr, height() * dpr);
	}

	Renderer mRenderer;
};


class GlWidget : public QOpenGLWidget {
public:
	~GlWidget() {
		makeCurrent();
		mRenderer.cleanup();
		doneCurrent();
	}

protected:
	void initializeGL() override { mRenderer.init(); }
	void paintGL() override {
		mRenderer.render(width() * devicePixelRatioF(), height() * devicePixelRatioF());
		update();
	}

	Renderer mRenderer;
};


static const char *qmlSource = R"(
import QtQuick

Rectangle {
	id: root
	width: 400; height: 300
	color: "#303050"

	Rectangle {
		width: 120; height: 120
		anchors.centerIn: parent
		gradient: Gradient {
			GradientStop { position: 0.0; color: "red" }
			GradientStop { position: 1.0; color: "yellow" }
		}
		RotationAnimator on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite }
	}

	Rectangle { width: 30; height: 30; color: "#00ff00"; anchors.top: parent.top; anchors.left: parent.left }
	Rectangle { width: 30; height: 30; color: "#0000ff"; anchors.bottom: parent.bottom; anchors.right: parent.right }

	Text {
		anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
		color: "white"; font.pixelSize: 16
		text: "Qt Quick on qfreerdp " + root.width + "x" + root.height
	}
}
)";


class App : public QObject {
public:
	App(const QString &mode, int count, bool resize, int cycleMs)
	: mMode(mode), mCount(count)
	{
		if (mMode == "quick") {
			mQmlFile.setFileTemplate(QDir::tempPath() + "/gltest-XXXXXX.qml");
			if (!mQmlFile.open())
				qFatal("unable to create the QML file");
			mQmlFile.write(qmlSource);
			mQmlFile.flush();
		}

		createWindows();

		if (resize) {
			connect(&mResizeTimer, &QTimer::timeout, this, &App::resizeWindows);
			mResizeTimer.start(150);
		}

		if (cycleMs > 0) {
			connect(&mCycleTimer, &QTimer::timeout, this, [this]() {
				destroyWindows();
				createWindows();
			});
			mCycleTimer.start(cycleMs);
		}
	}

	~App() { destroyWindows(); }

	void createWindows() {
		for (int i = 0; i < mCount; i++) {
			QRect geom(50 + i * 60, 50 + i * 60, 400, 300);
			QString title = QString("gltest %1 #%2").arg(mMode).arg(i);

			if (mMode == "window") {
				GlWindow *w = new GlWindow();
				w->setTitle(title);
				w->setGeometry(geom);
				w->show();
				mWindows.append(w);
			} else if (mMode == "quick") {
				QQuickView *v = new QQuickView();
				v->setTitle(title);
				v->setResizeMode(QQuickView::SizeRootObjectToView);
				v->setSource(QUrl::fromLocalFile(mQmlFile.fileName()));
				if (v->status() == QQuickView::Error)
					qFatal("QML error");
				v->setGeometry(geom);
				v->show();
				mWindows.append(v);
			} else if (mMode == "raster") {
				QLabel *top = new QLabel(QString("raster window #%1").arg(i));
				top->setWindowTitle(title);
				top->setAlignment(Qt::AlignCenter);
				top->setStyleSheet("background: #305030; color: white; font-size: 20px");
				top->setGeometry(geom);
				top->show();
				mWidgets.append(top);
			} else {
				QWidget *top = new QWidget();
				top->setWindowTitle(title);
				QVBoxLayout *layout = new QVBoxLayout(top);
				layout->addWidget(new QLabel("QOpenGLWidget below"));
				layout->addWidget(new GlWidget(), 1);
				top->setGeometry(geom);
				top->show();
				mWidgets.append(top);
			}
			g_created.fetchAndAddRelaxed(1);
		}
	}

	void destroyWindows() {
		qDeleteAll(mWindows);
		mWindows.clear();
		qDeleteAll(mWidgets);
		mWidgets.clear();
	}

	void resizeWindows() {
		auto *rng = QRandomGenerator::global();
		auto newGeom = [rng]() {
			return QRect(rng->bounded(0, 300), rng->bounded(0, 200), rng->bounded(1, 600), rng->bounded(1, 450));
		};
		for (QWindow *w : mWindows)
			w->setGeometry(newGeom());
		for (QWidget *w : mWidgets)
			w->setGeometry(newGeom());
	}

protected:
	QString mMode;
	int mCount;
	QTemporaryFile mQmlFile;
	QList<QWindow *> mWindows;
	QList<QWidget *> mWidgets;
	QTimer mResizeTimer;
	QTimer mCycleTimer;
};


int main(int argc, char **argv) {
	QApplication app(argc, argv);

	QCommandLineParser parser;
	parser.addHelpOption();
	QCommandLineOption modeOpt("mode", "window|quick|widget|raster", "mode", "window");
	QCommandLineOption windowsOpt("windows", "number of windows", "N", "1");
	QCommandLineOption resizeOpt("resize", "resize windows continuously");
	QCommandLineOption cycleOpt("cycle", "recreate the windows every MS ms", "MS", "0");
	QCommandLineOption durationOpt("duration", "quit after S seconds", "S", "0");
	parser.addOptions({ modeOpt, windowsOpt, resizeOpt, cycleOpt, durationOpt });
	parser.process(app);

	const QString mode = parser.value(modeOpt);
	if (mode != "window" && mode != "quick" && mode != "widget" && mode != "raster") {
		fprintf(stderr, "invalid mode %s\n", qPrintable(mode));
		return 1;
	}

	App *testApp = new App(mode, parser.value(windowsOpt).toInt(), parser.isSet(resizeOpt),
			parser.value(cycleOpt).toInt());

	QElapsedTimer elapsed;
	elapsed.start();

	QTimer stats;
	QObject::connect(&stats, &QTimer::timeout, [&]() {
		printf("[%5.1fs] frames=%d windows created=%d\n", elapsed.elapsed() / 1000.0,
				g_frames.loadRelaxed(), g_created.loadRelaxed());
		fflush(stdout);
	});
	stats.start(2000);

	const int duration = parser.value(durationOpt).toInt();
	if (duration > 0)
		QTimer::singleShot(duration * 1000, &app, &QCoreApplication::quit);

	int ret = app.exec();
	delete testApp;
	printf("done: frames=%d windows created=%d\n", g_frames.loadRelaxed(), g_created.loadRelaxed());
	return ret;
}
