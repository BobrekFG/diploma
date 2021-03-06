#include "mainwindow.h"
#include "ui_mainwindow.h"
using namespace cv;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->graphicsView->setScene(new QGraphicsScene(this));
    ui->graphicsView->scene()->addItem(&pixmap);
    findNN();
}

MainWindow::~MainWindow()
{
    delete ui;
}


const std::string darkplate_configuration	= "DarkPlate.cfg";
const std::string darkplate_best_weights	= "DarkPlate_best.weights";
const std::string darkplate_names			= "DarkPlate.names";
const size_t class_plate					= 0;
const auto font_face						= cv::FONT_HERSHEY_PLAIN;
const auto font_border						= 10.0;
const auto font_scale						= 3.5;
const auto font_thickness					= 2;
cv::Size network_size;
DarkHelp::NN nn;



void draw_label(const std::string & txt, cv::Mat & mat, const cv::Point & tl, const double factor = 1.0)
{
    const auto border		= factor * font_border;
    const auto scale		= factor * font_scale;
    const auto thickness	= static_cast<int>(std::max(1.0, factor * font_thickness));

    const cv::Size text_size = cv::getTextSize(txt, font_face, scale, thickness, nullptr);
    cv::Rect r(tl.x, tl.y - text_size.height - (border * 3), text_size.width + border * 2, text_size.height + border * 2);

    if (r.x + r.width	> mat.cols)	{	r.x = mat.cols - r.width	- border;	}
    if (r.y + r.height	> mat.rows)	{	r.y = mat.rows - r.height	- border;	}
    if (r.x < 0)					{	r.x = 0;								}
    if (r.y < 0)					{	r.y = 0;								}

    // lighten a box into which we'll write some text
    cv::Mat tmp;
    mat(r).convertTo(tmp, -1, 1, 125.0);

    const cv::Point point(border, tmp.rows - border);
    cv::putText(tmp, txt, point, font_face, scale, {0, 0, 0}, thickness, cv::LINE_AA);

    // copy the box and text back into the image
    tmp.copyTo(mat(r));

    return;
}


/// This is the 2nd stage detection.  By the time this is called, we have a smaller Roi, we no longer have the full frame.
void process_plate(DarkHelp::NN & nn, cv::Mat & plate, cv::Mat & output)
{
    auto results = nn.predict(plate);
    if (results.empty())
    {
        // nothing we can do with this image since no license plate was found
//		std::cout << "-> failed find a plate in this RoI" << std::endl;
        return;
    }

    // sort the results from left-to-right based on the mid-x point of each detected object
    std::sort(results.begin(), results.end(),
            [](const DarkHelp::PredictionResult & lhs, const DarkHelp::PredictionResult & rhs)
            {
                // put the "license plate" class first so the characters are drawn overtop of this class
                if (lhs.best_class == class_plate)	return true;
                if (rhs.best_class == class_plate)	return false;

                // otherwise, sort by the horizontal coordinate
                // (this obviously only works with license plates that consist of a single row of characters)
                return lhs.original_point.x < rhs.original_point.x;
            });

//	std::cout << "-> results: " << results << std::endl;

    cv::Point tl = results[0].rect.tl();

    // go over the plate class-by-class and build up what we think the license plate might be
    std::string license_plate;
    double probability = 0.0;
    for (const auto prediction : results)
    {
        if (prediction.rect.x < tl.x) tl.x = prediction.rect.x;
        if (prediction.rect.y < tl.y) tl.y = prediction.rect.y;

        probability += prediction.best_probability;
        if (prediction.best_class != class_plate)
        {
            license_plate += nn.names[prediction.best_class];
        }
    }

    // store the sorted results back in DarkHelp so the annotations are drawn with the license plate first
    nn.prediction_results = results;
    cv::Mat mat = nn.annotate();

    if (license_plate.empty() == false)
    {
        const std::string label = license_plate + " [" + std::to_string((size_t)std::round(100.0 * probability / results.size())) + "%]";
        std::cout << "-> license plate: " << label << std::endl;

        draw_label(license_plate /* label */, mat, tl);
    }

    // copy the annotated RoI back into the output image to be used when writing the video
    mat.copyTo(output);

    return;
}


/** Process a single license plate located within the given prediction.
 * This means we build a RoI and apply it the rectangle to both the frame and the output image.
 */
void process_plate(DarkHelp::NN & nn, cv::Mat & frame, const DarkHelp::PredictionResult & prediction, cv::Mat & output_frame)
{
    cv::Rect roi = prediction.rect;

    if (roi.width < 1 or roi.height < 1)
    {
        std::cout << "-> ignoring impossibly small plate (x=" << roi.x << " y=" << roi.y << " w=" << roi.width << " h=" << roi.height << ")" << std::endl;
        return;
    }

    // increase the RoI to match the network dimensions, but stay within the bounds of the frame
    if (roi.width >= network_size.width or roi.height >= network_size.height)
    {
        // something is wrong with this plate, since it seems to be the same size or bigger than the original frame size!
        std::cout << "-> ignoring too-big plate (x=" << roi.x << " y=" << roi.y << " w=" << roi.width << " h=" << roi.height << ")" << std::endl;
        return;
    }

    const double dx = 0.5 * (network_size.width		- roi.width	);
    const double dy = 0.5 * (network_size.height	- roi.height);

    roi.x		-= std::floor(dx);
    roi.y		-= std::floor(dy);
    roi.width	+= std::ceil(dx * 2.0);
    roi.height	+= std::ceil(dy * 2.0);

    // check all the edges and reposition the RoI if necessary
    if (roi.x < 0)							roi.x = 0;
    if (roi.y < 0)							roi.y = 0;
    if (roi.x + roi.width	> frame.cols)	roi.x = frame.cols - roi.width;
    if (roi.y + roi.height	> frame.rows)	roi.y = frame.rows - roi.height;

    #if 0
    std::cout	<< "-> plate found: " << prediction << std::endl
                << "-> roi: x=" << roi.x << " y=" << roi.y << " w=" << roi.width << " h=" << roi.height << std::endl;
    #endif

    // the RoI should now be the same size as the network dimensions, and all edges should be valid
    cv::Mat plate = frame(roi);
    cv::Mat output = output_frame(roi);
    process_plate(nn, plate, output);

    return;
}


cv::Mat process_frame(DarkHelp::NN & nn, cv::Mat & frame)
{
    cv::Mat output_frame = frame.clone();

    // we need to find all the license plates in the image
    auto result = nn.predict(frame);
    for (const auto & prediction : result)
    {
        // at this stage we're only interested in the "license plate" class, ignore everything else
        if (prediction.best_class == class_plate)
        {
            process_plate(nn, frame, prediction, output_frame);
            //ffffffffffffffffff img roi
        }
    }

    return output_frame;
}


void findNN()
{


                bool initialization_done = false;
                for (const auto path : {"./", "../", "../../", "nn/", "../nn/", "../../nn/"})
                {
                    const auto fn = path + darkplate_configuration;
                    std::cout << "Looking for " << fn << std::endl;
                    std::ifstream ifs(fn);
                    if (ifs.is_open())
                    {
                        const DarkHelp::EDriver driver = DarkHelp::EDriver::kDarknet;

                        ifs.close();
                        std::cout << "Found neural network: " << fn << std::endl;
                        const std::string cfg		= fn;
                        const std::string names		= path + darkplate_names;
                        const std::string weights	= path + darkplate_best_weights;
                        nn.init(cfg, weights, names, true, driver);
                        nn.config.enable_debug					= false;
                        nn.config.annotation_auto_hide_labels	= false;
                        nn.config.annotation_include_duration	= false;
                        nn.config.annotation_include_timestamp	= false;
                        nn.config.enable_tiles					= false;
                        nn.config.combine_tile_predictions		= true;
                        nn.config.include_all_names				= true;
                        nn.config.names_include_percentage		= true;
                        nn.config.threshold						= 0.25;
                        nn.config.sort_predictions				= DarkHelp::ESort::kUnsorted;
                        initialization_done						= true;
                        break;
                    }
                }
                if (initialization_done == false)
                {
                    throw std::runtime_error("failed to find the neural network DarkPlate.cfg");
                }

                // remember the size of the network, since we'll need to crop plates to this exact size
                network_size = nn.network_size();
}



void MainWindow::on_startBtn_pressed()
{
    if(cap.isOpened())
    {
        ui->startBtn->setText("Start");
        cap.release();
        return;
    }

    bool isCamera;
    int cameraIndex = ui->videoEdit->text().toInt(&isCamera);
    std::string filename = ui->videoEdit->text().trimmed().toStdString();
    if(isCamera)
    {
        if(!cap.open(cameraIndex))
        {
            QMessageBox::critical(this,
                                  "Camera Error",
                                  "Make sure you entered a correct camera index,"
                                  "<br>or that the camera is not being accessed by another program!");
            return;
        }
    }
    else
    {
        if(!cap.open(filename))
        {
            QMessageBox::critical(this,
                                  "Video Error",
                                  "Make sure you entered a correct and supported video file path,"
                                  "<br>or a correct RTSP feed URL!");
            return;
        }
    }

    ui->startBtn->setText("Stop");

    const double width		= cap.get(cv::VideoCaptureProperties::CAP_PROP_FRAME_WIDTH	);
    const double height		= cap.get(cv::VideoCaptureProperties::CAP_PROP_FRAME_HEIGHT	);
    const double frames		= cap.get(cv::VideoCaptureProperties::CAP_PROP_FRAME_COUNT	);
    const double fps		= cap.get(cv::VideoCaptureProperties::CAP_PROP_FPS			);
    const size_t round_fps	= std::round(fps);

    std::cout	<< "-> " << static_cast<size_t>(width) << " x " << static_cast<size_t>(height) << " @ " << fps << " FPS" << std::endl
                << "-> " << frames << " frames (" << static_cast<size_t>(std::round(frames / fps)) << " seconds)" << std::endl;

    if (width < network_size.width or height < network_size.height)
    {
        std::cout << "ERROR: \"" << filename << "\" [" << width << " x " << height << "] is smaller than the network size " << network_size << "!" << std::endl;
        return;
    }

    size_t frame_counter = 0;
    while (cap.isOpened())
    {
        const auto t1 = std::chrono::high_resolution_clock::now();

        Mat frame;
        cap >> frame;
        if (!frame.empty())
        {

            if (frame_counter % round_fps == 0)
            {
                std::cout << "\r-> frame #" << frame_counter << " (" << std::round(100 * frame_counter / frames) << "%)" << std::flush;
            }

            auto output_frame = process_frame(nn, frame);
            const auto t2 = std::chrono::high_resolution_clock::now();

            // "steal" the duration format function in DarkHelp
            draw_label(DarkHelp::duration_string(t2 - t1), output_frame, cv::Point(0, 0), 0.5);
            QImage qimg(output_frame.data,
                        output_frame.cols,
                        output_frame.rows,
                        output_frame.step,
                        QImage::Format_RGB888);
            pixmap.setPixmap( QPixmap::fromImage(qimg.rgbSwapped()) );
            ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);

            frame_counter ++;
        }

        qApp->processEvents();
    }

    ui->startBtn->setText("Start");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(cap.isOpened())
    {
        QMessageBox::warning(this,
                             "Warning",
                             "Stop the video before closing the application!");
        event->ignore();
    }
    else
    {
        event->accept();
    }
}
