// This program does SyN registration with hard-coded parameters.
// The whole registration process is included in "simpleSynReg" function.

// The Usage:
// This program does not have any flag. You should just put the arguments after the program name.
/*
~/simpleSynRegistration
 fixed image
 moving image
 initial transform
 output prefix file name
*/

#include "antsUtilities.h"
#include "itkantsRegistrationHelper.h"

namespace ants
{
typedef  ants::RegistrationHelper<3>                    RegistrationHelperType;
typedef  RegistrationHelperType::ImageType              ImageType;
typedef  RegistrationHelperType::CompositeTransformType CompositeTransformType;

CompositeTransformType::TransformTypePointer
simpleSynReg( ImageType::Pointer & fixedImage, ImageType::Pointer & movingImage,
              const CompositeTransformType::Pointer compositeInitialTransform )
{
  RegistrationHelperType::Pointer regHelper = RegistrationHelperType::New();

  std::string                               whichMetric = "mattes";
  RegistrationHelperType::MetricEnumeration curMetric = regHelper->StringToMetricType(whichMetric);
  float                                     lowerQuantile = 0.0;
  float                                     upperQuantile = 1.0;
  bool                                      doWinsorize(false);

  regHelper->SetWinsorizeImageIntensities(doWinsorize, lowerQuantile, upperQuantile);

  bool doHistogramMatch(true);
  regHelper->SetUseHistogramMatching(doHistogramMatch);

  bool doEstimateLearningRateAtEachIteration = true;
  regHelper->SetDoEstimateLearningRateAtEachIteration( doEstimateLearningRateAtEachIteration );

  std::vector<std::vector<unsigned int> > iterationList;
  std::vector<double>                     convergenceThresholdList;
  std::vector<unsigned int>               convergenceWindowSizeList;
  std::vector<std::vector<unsigned int> > shrinkFactorsList;
  std::vector<std::vector<float> >        smoothingSigmasList;

  std::vector<unsigned int> iterations(3);
  iterations[0] = 100; iterations[1] = 70; iterations[2] = 20;
  double       convergenceThreshold = 1e-6;
  unsigned int convergenceWindowSize = 10;
  antscout << "  number of levels = 3 " << std::endl;
  iterationList.push_back(iterations);
  convergenceThresholdList.push_back(convergenceThreshold);
  convergenceWindowSizeList.push_back(convergenceWindowSize);

  std::vector<unsigned int> factors(3);
  factors[0] = 3; factors[1] = 2; factors[2] = 1;
  shrinkFactorsList.push_back(factors);

  std::vector<float> sigmas(3);
  sigmas[0] = 2; sigmas[1] = 1; sigmas[2] = 0;
  smoothingSigmasList.push_back(sigmas);

  float                                    samplingPercentage = 1.0;
  RegistrationHelperType::SamplingStrategy samplingStrategy = RegistrationHelperType::none;
  unsigned int                             binOption = 200;
  regHelper->AddMetric(curMetric, fixedImage, movingImage, 1.0, samplingStrategy, binOption, 1, samplingPercentage);

  float       learningRate = 0.25;
  const float varianceForUpdateField = 3.0;
  const float varianceForTotalField = 0.0;

  regHelper->AddSyNTransform(learningRate, varianceForUpdateField, varianceForTotalField);
  regHelper->SetMovingInitialTransform( compositeInitialTransform );
  regHelper->SetIterations( iterationList );
  regHelper->SetConvergenceWindowSizes( convergenceWindowSizeList );
  regHelper->SetConvergenceThresholds( convergenceThresholdList );
  regHelper->SetSmoothingSigmas( smoothingSigmasList );
  regHelper->SetShrinkFactors( shrinkFactorsList );
  if( regHelper->DoRegistration() == EXIT_FAILURE )
    {
    antscout << "FATAL ERROR: REGISTRATION PROCESS WAS UNSUCCESSFUL" << std::endl;
    }
  // Get the output transform
  CompositeTransformType::Pointer outputCompositeTransform = regHelper->GetCompositeTransform();
  // write out transform actually computed, so skip the initial transform
  CompositeTransformType::TransformTypePointer resultTransform = outputCompositeTransform->GetNthTransform( 1 );

  return resultTransform;
}

int simpleSynRegistration( std::vector<std::string> args, std::ostream* out_stream = NULL )
{
  // the arguments coming in as 'args' is a replacement for the standard (argc,argv) format
  // Just notice that the argv[i] equals to args[i-1]
  // and the argc equals:
  int argc = args.size() + 1;

  if( argc != 5 )
    {
    std::cerr << "Usage: simpleSynRegistration\n"
              <<
      "<Fixed Image> , <Moving Image> , <Initial Transform> , <Output prefix file name without any extension>"
              << std::endl;
    return EXIT_FAILURE;
    }

  antscout->set_stream( out_stream );

  ImageType::Pointer fixedImage;
  ImageType::Pointer movingImage;
  // ========read the fixed image
  typedef itk::ImageFileReader<ImageType> ImageReaderType;
  ImageReaderType::Pointer fixedImageReader = ImageReaderType::New();
  fixedImageReader->SetFileName( args[0] );
  fixedImageReader->Update();
  fixedImage = fixedImageReader->GetOutput();
  try
    {
    fixedImage->Update();
    }
  catch( itk::ExceptionObject & excp )
    {
    return EXIT_FAILURE;
    }
  // ==========read the moving image
  ImageReaderType::Pointer movingImageReader = ImageReaderType::New();
  movingImageReader->SetFileName( args[1] );
  movingImageReader->Update();
  movingImage = movingImageReader->GetOutput();
  try
    {
    movingImage->Update();
    }
  catch( itk::ExceptionObject & excp )
    {
    return EXIT_FAILURE;
    }

  antscout << "  fixed image: " << args[0] << std::endl;
  antscout << "  moving image: " << args[1] << std::endl;

  // ===========Read the initial transform and write that in a composite transform
  typedef RegistrationHelperType::TransformType TransformType;
  TransformType::Pointer initialTransform = itk::ants::ReadTransform<3>( args[2] );
  if( initialTransform.IsNull() )
    {
    antscout << "Can't read initial transform " << std::endl;
    return NULL;
    }
  CompositeTransformType::Pointer compositeInitialTransform = CompositeTransformType::New();
  compositeInitialTransform->AddTransform( initialTransform );

  // =========write the output transform
  // compute the output transform by calling the "simpleSynReg" function
  CompositeTransformType::TransformTypePointer outputTransform = simpleSynReg( fixedImage, movingImage,
                                                                               compositeInitialTransform );

  antscout << "***** Ready to write the results ...\n" << std::endl;
  std::stringstream outputFileName;
  outputFileName << args[3] << "Warp.nii.gz";
  itk::ants::WriteTransform<3>( outputTransform, outputFileName.str() );

  // compute and write the inverse of the output transform
  bool writeInverse(true);
  if( writeInverse )
    {
    typedef RegistrationHelperType::DisplacementFieldTransformType DisplacementFieldTransformType;
    DisplacementFieldTransformType::Pointer dispTransform =
      dynamic_cast<DisplacementFieldTransformType *>(outputTransform.GetPointer() );
    typedef DisplacementFieldTransformType::DisplacementFieldType DisplacementFieldType;
    std::stringstream outputInverseFileName;
    outputInverseFileName << args[3] << "InverseWarp.nii.gz";
    typedef itk::ImageFileWriter<DisplacementFieldType> InverseWriterType;
    InverseWriterType::Pointer inverseWriter = InverseWriterType::New();
    inverseWriter->SetInput( dispTransform->GetInverseDisplacementField() );
    inverseWriter->SetFileName( outputInverseFileName.str().c_str() );
    inverseWriter->Update();
    }

  return EXIT_SUCCESS;
}
} // namespace ants
