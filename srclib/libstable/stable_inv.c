/* stable/stable_inv.h
 *
 * Functions to calculate the quantile function of alpha-stable
 * distributions. Based on the developed method for CDF evaluation.
 * Code fractions based on code in [1].
 *
 * [1] Mark S. Veillete. Alpha-Stable Distributions in MATLAB
 *     http://math.bu.edu/people/mveillet/research.html
 *
 * Copyright (C) 2013. Javier Royuela del Val
 *                     Federico Simmross Wattenberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  Javier Royuela del Val.
 *  E.T.S.I. Telecomunicación
 *  Universidad de Valladolid
 *  Paseo de Belén 15, 47002 Valladolid, Spain.
 *  jroyval@lpi.tel.uva.es
 */
#include "stable_api.h"
#include "stable_integration.h"
#include <gsl/gsl_roots.h>
#include <gsl/gsl_cdf.h>

#include "methods.h"
#include <pthread.h>

const double precalc[9][6][20] = {{{ -1.890857122067030e+06, -1.074884919696010e+03, -9.039223076384694e+01, -2.645987890965098e+01, -1.274134564492298e+01, -7.864009406553024e+00, -5.591791397752695e+00, -4.343949435866958e+00, -3.580521076832391e+00, -3.077683537175253e+00, -2.729262880847457e+00, -2.479627528870857e+00, -2.297138304998905e+00, -2.162196365947914e+00, -2.061462692277420e+00, -1.985261982958637e+00, -1.926542865732525e+00, -1.880296841910385e+00, -1.843044812063057e+00, -1.812387604873646e+00},
                             {-1.476366405440763e+05, -2.961237538429159e+02, -3.771873580263473e+01, -1.357404219788403e+01, -7.411052003232824e+00, -4.988799898398770e+00, -3.787942909197120e+00, -3.103035515608863e+00, -2.675942594722292e+00, -2.394177022026705e+00, -2.202290611202918e+00, -2.070075681428623e+00, -1.979193969170630e+00, -1.917168989568703e+00, -1.875099179801364e+00, -1.846852935880107e+00, -1.828439745755405e+00, -1.817388844989596e+00, -1.812268962543248e+00, -1.812387604873646e+00},
                             {-4.686998894118387e+03, -5.145071882481552e+01, -1.151718246460839e+01, -5.524535336243413e+00, -3.611648531595958e+00, -2.762379160216148e+00, -2.313577186902494e+00, -2.052416861482463e+00, -1.893403771865641e+00, -1.796585983161395e+00, -1.740583121589162e+00, -1.711775396141753e+00, -1.700465158047576e+00, -1.700212465596452e+00, -1.707238269631509e+00, -1.719534615317151e+00, -1.736176665562027e+00, -1.756931455967477e+00, -1.782079727531726e+00, -1.812387604873646e+00},
                             {-2.104710824345458e+01, -3.379418096823576e+00, -1.919928049616870e+00, -1.508399002681057e+00, -1.348510542803496e+00, -1.284465355994317e+00, -1.267907903071982e+00, -1.279742001004255e+00, -1.309886183701422e+00, -1.349392554642457e+00, -1.391753942957071e+00, -1.434304119387730e+00, -1.476453646904256e+00, -1.518446568503842e+00, -1.560864595722380e+00, -1.604464355709833e+00, -1.650152416312346e+00, -1.699029550621646e+00, -1.752489822658308e+00, -1.812387604873646e+00},
                             {-1.267075422596289e-01, -2.597188113311268e-01, -4.004811495862077e-01, -5.385024279816432e-01, -6.642916520777534e-01, -7.754208907962602e-01, -8.732998811318613e-01, -9.604322013853581e-01, -1.039287445657237e+00, -1.111986321525904e+00, -1.180285915835185e+00, -1.245653509438976e+00, -1.309356535558631e+00, -1.372547245869795e+00, -1.436342854982504e+00, -1.501904088536648e+00, -1.570525854475943e+00, -1.643747672313277e+00, -1.723509779436442e+00, -1.812387604873646e+00},
                             {-1.582153175255304e-01, -3.110425775503970e-01, -4.383733961816599e-01, -5.421475800719634e-01, -6.303884905318050e-01, -7.089178961038225e-01, -7.814055112235458e-01, -8.502117698317242e-01, -9.169548634355569e-01, -9.828374636178471e-01, -1.048835660976022e+00, -1.115815771583362e+00, -1.184614345408666e+00, -1.256100352867799e+00, -1.331235978799527e+00, -1.411143947581252e+00, -1.497190629447853e+00, -1.591104422133556e+00, -1.695147748117837e+00, -1.812387604873646e+00}},
                             {{-4.738866777987500e+02, -1.684460387562537e+01, -5.619926961081743e+00, -3.281734135829228e+00, -2.397479160864619e+00, -1.959508008521143e+00, -1.708174380583835e+00, -1.550822278332538e+00, -1.447013328833974e+00, -1.376381920471173e+00, -1.327391983207241e+00, -1.292811209009340e+00, -1.267812588403031e+00, -1.249132310044230e+00, -1.234616432819130e+00, -1.222879780072203e+00, -1.213041554808853e+00, -1.204541064608597e+00, -1.197016952370690e+00, -1.190232162899989e+00},
                             {-2.185953347160669e+01, -3.543320127025984e+00, -1.977029667649595e+00, -1.507632281031653e+00, -1.303310228044346e+00, -1.199548019673933e+00, -1.144166826374866e+00, -1.115692821970145e+00, -1.103448361903579e+00, -1.101126400280696e+00, -1.104531584444055e+00, -1.110930462397609e+00, -1.118760810700929e+00, -1.127268239360369e+00, -1.136171639806347e+00, -1.145449097190615e+00, -1.155224344271089e+00, -1.165719407748303e+00, -1.177246763148178e+00, -1.190232162899989e+00},
                             {-2.681009914911080e-01, -4.350930213152404e-01, -5.305212880041126e-01, -6.015232065896753e-01, -6.620641788021128e-01, -7.174026993828067e-01, -7.694003004766365e-01, -8.178267862332173e-01, -8.615585464741182e-01, -9.003104216523169e-01, -9.347554970493899e-01, -9.658656088352816e-01, -9.945788535033495e-01, -1.021718797792234e+00, -1.048005562158225e+00, -1.074094694885961e+00, -1.100624477495892e+00, -1.128270402039747e+00, -1.157812818875688e+00, -1.190232162899989e+00},
                             {-9.503065419472154e-02, -1.947070824738389e-01, -2.987136341021804e-01, -3.973064532664002e-01, -4.838698271554803e-01, -5.579448431371428e-01, -6.215822273361273e-01, -6.771753949313707e-01, -7.267793058476849e-01, -7.720164852674839e-01, -8.141486817740096e-01, -8.541760575495752e-01, -8.929234555236560e-01, -9.311104141820112e-01, -9.694099704722252e-01, -1.008502023575024e+00, -1.049129636922346e+00, -1.092166845038550e+00, -1.138712425453996e+00, -1.190232162899989e+00},
                             {-1.264483719244014e-01, -2.437377726529247e-01, -3.333750988387906e-01, -4.016893641684894e-01, -4.577316520822721e-01, -5.069548741156986e-01, -5.523620701546919e-01, -5.956554729327528e-01, -6.378655338388568e-01, -6.796745661620428e-01, -7.215886443544494e-01, -7.640354693071291e-01, -8.074261467088205e-01, -8.522003643607233e-01, -8.988670244927734e-01, -9.480479125009214e-01, -1.000533792677121e+00, -1.057363229272293e+00, -1.119941850176443e+00, -1.190232162899989e+00},
                             {-1.526287733702501e-01, -2.498255243669921e-01, -3.063859169446500e-01, -3.504924054764082e-01, -3.911254396222550e-01, -4.309657384679277e-01, -4.709130419301468e-01, -5.113624096299824e-01, -5.525816075847192e-01, -5.948321009341774e-01, -6.384119892912432e-01, -6.836776839822375e-01, -7.310612144698296e-01, -7.810921001396979e-01, -8.344269070778757e-01, -8.918931068397437e-01, -9.545526172382969e-01, -1.023797332562095e+00, -1.101496412960141e+00, -1.190232162899989e+00}},
                             {{-1.354883142615948e+00, -8.855778500552980e-01, -7.773858277863260e-01, -7.357727812399337e-01, -7.181850957003714e-01, -7.120493514301658e-01, -7.121454153857569e-01, -7.157018373526386e-01, -7.209253714350538e-01, -7.265425280053609e-01, -7.317075569303094e-01, -7.359762286696208e-01, -7.392122467978279e-01, -7.414607677550720e-01, -7.428480570989012e-01, -7.435216571211187e-01, -7.436225251216279e-01, -7.432733099840527e-01, -7.425762029730668e-01, -7.416143171871161e-01},
                             {-5.193811327974376e-02, -1.633949875159595e-01, -2.617724006156590e-01, -3.392619822712012e-01, -4.018554923458003e-01, -4.539746445467862e-01, -4.979328472153985e-01, -5.348184073267473e-01, -5.654705188376931e-01, -5.909430146259388e-01, -6.123665499489599e-01, -6.307488506465194e-01, -6.469130897780404e-01, -6.615145568123281e-01, -6.750798357120451e-01, -6.880470899358724e-01, -7.008026232247697e-01, -7.137148222421971e-01, -7.271697520465581e-01, -7.416143171871161e-01},
                             {-6.335376612981386e-02, -1.297738965263227e-01, -1.985319371835911e-01, -2.624863717000360e-01, -3.174865471926985e-01, -3.637544360366539e-01, -4.030045272659678e-01, -4.369896090801292e-01, -4.671253359013797e-01, -4.944847533335236e-01, -5.198770070249209e-01, -5.439265161390062e-01, -5.671356857543234e-01, -5.899325077218274e-01, -6.127077038151078e-01, -6.358474023877762e-01, -6.597648782206755e-01, -6.849381555866478e-01, -7.119602076523737e-01, -7.416143171871161e-01},
                             {-9.460338726038994e-02, -1.756165596280472e-01, -2.282691311262980e-01, -2.638458905915733e-01, -2.918110046315503e-01, -3.167744873288179e-01, -3.408290016876749e-01, -3.649204420006245e-01, -3.894754728525021e-01, -4.146904022890949e-01, -4.406707089221509e-01, -4.675033009839270e-01, -4.952960990683358e-01, -5.242037261193876e-01, -5.544463409264927e-01, -5.863313160876512e-01, -6.202819599064874e-01, -6.568811178840162e-01, -6.969403639254603e-01, -7.416143171871159e-01},
                             {-1.158003423724520e-01, -1.620942232133271e-01, -1.790483132028017e-01, -1.937097725890709e-01, -2.109729530977958e-01, -2.311198638992638e-01, -2.537077422985343e-01, -2.783252370301364e-01, -3.047045003309861e-01, -3.327092628454751e-01, -3.623063449447594e-01, -3.935470145089454e-01, -4.265595391976379e-01, -4.615525703717921e-01, -4.988293297210071e-01, -5.388134824040952e-01, -5.820906647738434e-01, -6.294732446564461e-01, -6.821024214831549e-01, -7.416143171871159e-01},
                             {-5.695213481951577e-02, -2.485009114767256e-02, -2.455774348005581e-02, -4.243720620421176e-02, -6.906960852184874e-02, -1.000745485866474e-01, -1.334091111747126e-01, -1.681287272131953e-01, -2.038409527302062e-01, -2.404547731975402e-01, -2.780623638274261e-01, -3.168837529800063e-01, -3.572466721186688e-01, -3.995862986780706e-01, -4.444626893956575e-01, -4.925935308416445e-01, -5.449092276644302e-01, -6.026377433551201e-01, -6.674379829825384e-01, -7.416143171871159e-01}},
                            {{-4.719005698760254e-03, -5.039419714218448e-02, -1.108600074872916e-01, -1.646393852283324e-01, -2.088895889525075e-01, -2.445873831127209e-01, -2.729819770922066e-01, -2.951510874462016e-01, -3.121233685073350e-01, -3.249196962329062e-01, -3.344714240325961e-01, -3.415532212363377e-01, -3.467713617249639e-01, -3.505859000173167e-01, -3.533413466958321e-01, -3.552947623689004e-01, -3.566384591258251e-01, -3.575167387322836e-01, -3.580387843935552e-01, -3.582869092425832e-01},
                             {-3.167687806490741e-02, -6.488347295237770e-02, -9.913854730442322e-02, -1.306663969875579e-01, -1.574578108363950e-01, -1.797875581290475e-01, -1.986122400020671e-01, -2.148458045681510e-01, -2.292024720743768e-01, -2.422125650878785e-01, -2.542699931601989e-01, -2.656748454748664e-01, -2.766656461455947e-01, -2.874428940341864e-01, -2.981872822548070e-01, -3.090746307371333e-01, -3.202900038682522e-01, -3.320450798333745e-01, -3.445973947956370e-01, -3.582869092425832e-01},
                             {-6.256908981229170e-02, -1.058190431028687e-01, -1.215669874255146e-01, -1.261149689648148e-01, -1.284283108027729e-01, -1.318108373643454e-01, -1.372885008966837e-01, -1.450218673440198e-01, -1.548461140242879e-01, -1.664940537646226e-01, -1.796994139325742e-01, -1.942454974557965e-01, -2.099854734361004e-01, -2.268483937252861e-01, -2.448403779828917e-01, -2.640470286750166e-01, -2.846415660837839e-01, -3.069024734642628e-01, -3.312464672828315e-01, -3.582869092425832e-01},
                             {-7.132464704948761e-02, -5.885471032381771e-02, -3.846810486653290e-02, -2.801768649688129e-02, -2.615407079824540e-02, -3.037902421859952e-02, -3.894619676380785e-02, -5.076849313651704e-02, -6.518223105549245e-02, -8.178056142331483e-02, -1.003134231215546e-01, -1.206343411798188e-01, -1.426762955132322e-01, -1.664453845103147e-01, -1.920257997377931e-01, -2.195942670864279e-01, -2.494428999135824e-01, -2.820166786810741e-01, -3.179740384308457e-01, -3.582869092425832e-01},
                              {1.186775035989228e-01,  1.847231744541209e-01,  1.899666578065291e-01,  1.756596652192159e-01,  1.538218851318199e-01,  1.287679439328719e-01,  1.022243387982872e-01,  7.488543991005173e-02,  4.698265181928261e-02,  1.852002327642577e-02, -1.062008675791458e-02, -4.062891141128176e-02, -7.175196683590498e-02, -1.042870733773311e-01, -1.385948877988075e-01, -1.751227987938045e-01, -2.144432379167035e-01, -2.573138196343415e-01, -3.047716553689650e-01, -3.582869092425832e-01},
                              {1.359937191266603e+00,  7.928324704017256e-01,  6.068350758065271e-01,  4.949176895753282e-01,  4.117787224185477e-01,  3.435869264264112e-01,  2.844376471729288e-01,  2.312306852681522e-01,  1.820841981890349e-01,  1.357181057787019e-01,  9.117291945474759e-02,  4.766184332000264e-02,  4.481886485253039e-03, -3.904933750228177e-02, -8.364689014849616e-02, -1.301133939768983e-01, -1.794049920724848e-01, -2.327202766583559e-01, -2.916310469293936e-01, -3.582869092425832e-01}},
                             {{0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00,  0.000000000000000e+00},
                             {-2.998229841415443e-02, -3.235136568035350e-02, -1.058934315424071e-02,  1.472786013654386e-02,  3.649529125352272e-02,  5.320761222262883e-02,  6.497369053185199e-02,  7.235439352353751e-02,  7.603800885095309e-02,  7.671459793802816e-02,  7.500001602159387e-02,  7.139599669434762e-02,  6.628276247821394e-02,  5.992932695316782e-02,  5.250925428603021e-02,  4.411421669339249e-02,  3.476266163507976e-02,  2.439917920106283e-02,  1.289010976694223e-02,  0.000000000000000e+00},
                             {-4.911181618214269e-04,  7.928758678692660e-02,  1.295711243349632e-01,  1.575625247967377e-01,  1.726794061650541e-01,  1.799982238321182e-01,  1.821699713013862e-01,  1.806145618464317e-01,  1.761248753943454e-01,  1.691770293512301e-01,  1.600901411017374e-01,  1.491003610537801e-01,  1.363865273697878e-01,  1.220722641614886e-01,  1.062191001109524e-01,  8.881586460416716e-02,  6.976629777350905e-02,  4.886974404989612e-02,  2.578932638717129e-02,  0.000000000000000e+00},
                              {6.444732609572413e-01,  5.412205715497974e-01,  4.864603927210872e-01,  4.457073928551408e-01,  4.118964225372133e-01,  3.823074983529713e-01,  3.554905959697276e-01,  3.305043126978712e-01,  3.066571802106021e-01,  2.834017043112906e-01,  2.602853501366307e-01,  2.369238065872132e-01,  2.129824521942899e-01,  1.881563959610275e-01,  1.621474808586950e-01,  1.346349888095220e-01,  1.052403813710735e-01,  7.348119932151805e-02,  3.870673240105876e-02,  0.000000000000000e+00},
                              {4.884639795042095e+00,  1.686842470765597e+00,  1.132342494635284e+00,  8.944978064032267e-01,  7.538011200000044e-01,  6.558265419066330e-01,  5.806408912949470e-01,  5.191065509143589e-01,  4.663489244354866e-01,  4.194539705064985e-01,  3.765099860312678e-01,  3.361566147323812e-01,  2.973499640484341e-01,  2.592283952427927e-01,  2.210255604589869e-01,  1.820030836908522e-01,  1.413881485626739e-01,  9.829989964989198e-02,  5.165115573609639e-02,  0.000000000000000e+00},
                              {2.410567057697245e+01,  4.005534670805399e+00,  2.144263118197206e+00,  1.518214626927320e+00,  1.198109338317733e+00,  9.966378800612080e-01,  8.532685386168033e-01,  7.427048697651345e-01,  6.524693172360032e-01,  5.756299950589361e-01,  5.079606300067100e-01,  4.466711396792393e-01,  3.897746494263863e-01,  3.357416130711989e-01,  2.832892169418335e-01,  2.312355801087936e-01,  1.783807793433976e-01,  1.233869208812706e-01,  6.463145748462040e-02,  9.714451465470120e-17}},
                             {{4.719005698760275e-03,  5.039419714218456e-02,  1.108600074872919e-01,  1.646393852283322e-01,  2.088895889525074e-01,  2.445873831127209e-01,  2.729819770922065e-01,  2.951510874462016e-01,  3.121233685073347e-01,  3.249196962329060e-01,  3.344714240325963e-01,  3.415532212363379e-01,  3.467713617249641e-01,  3.505859000173170e-01,  3.533413466958320e-01,  3.552947623689000e-01,  3.566384591258254e-01,  3.575167387322835e-01,  3.580387843935554e-01,  3.582869092425831e-01},
                              {1.944613194060750e-01,  3.117984496788369e-01,  3.615078716560812e-01,  3.879646155737581e-01,  4.042606354602197e-01,  4.152379986226543e-01,  4.229018705591941e-01,  4.280900470005300e-01,  4.311273812611276e-01,  4.321442286112657e-01,  4.312423594533669e-01,  4.285591238013830e-01,  4.242644840754073e-01,  4.185310514289916e-01,  4.115050794489342e-01,  4.032875933324668e-01,  3.939222836649399e-01,  3.833860261287606e-01,  3.715758694363207e-01,  3.582869092425831e-01},
                              {3.045958300133999e+00,  1.315675725057089e+00,  9.757973307352019e-01,  8.294361410388060e-01,  7.456405896421689e-01,  6.900226415397631e-01,  6.495436520935480e-01,  6.180526887451320e-01,  5.921654464012007e-01,  5.697923159645174e-01,  5.495326577846258e-01,  5.304020801294532e-01,  5.116943409858906e-01,  4.928954730588648e-01,  4.736165965702772e-01,  4.535361612745278e-01,  4.323485980953122e-01,  4.097162006469898e-01,  3.852184728042033e-01,  3.582869092425835e-01},
                              {2.339312510820383e+01,  3.858569195402605e+00,  2.091507439545032e+00,  1.515362821077606e+00,  1.231804842218289e+00,  1.060749495885882e+00,  9.442937075476816e-01,  8.583603822642385e-01,  7.911221543980916e-01,  7.360251815557063e-01,  6.890778676134198e-01,  6.476526200515113e-01,  6.099033923678876e-01,  5.744600864566568e-01,  5.402514096915735e-01,  5.063904595668142e-01,  4.720865286037160e-01,  4.365637761840112e-01,  3.989743423180101e-01,  3.582869092425835e-01},
                              {1.231812404655975e+02,  9.151933726881031e+00,  3.856468345925451e+00,  2.470027172456050e+00,  1.862167039303084e+00,  1.521067254392224e+00,  1.300039377551776e+00,  1.142711537858461e+00,  1.023045102736937e+00,  9.273664178094935e-01,  8.477633920324498e-01,  7.792812067953944e-01,  7.185943530039393e-01,  6.633207377171386e-01,  6.116407715135426e-01,  5.620594176198462e-01,  5.132627179036522e-01,  4.639774715385669e-01,  4.128508865888630e-01,  3.582869092425835e-01},
                              {5.049829135345403e+02,  1.890722475322573e+01,  6.427275565975617e+00,  3.715903402980179e+00,  2.636417882085815e+00,  2.065989355542487e+00,  1.711228437455139e+00,  1.466088158475343e+00,  1.283765226486882e+00,  1.140575450959062e+00,  1.023262411940948e+00,  9.237922892835746e-01,  8.369566524681974e-01,  7.591595457820643e-01,  6.877508180861301e-01,  6.206265009880273e-01,  5.559603894356728e-01,  4.919976875425384e-01,  4.268552022160075e-01,  3.582869092425835e-01}},
                             {{1.354883142615939e+00,  8.855778500552969e-01,  7.773858277863266e-01,  7.357727812399328e-01,  7.181850957003700e-01,  7.120493514301658e-01,  7.121454153857567e-01,  7.157018373526381e-01,  7.209253714350531e-01,  7.265425280053608e-01,  7.317075569303093e-01,  7.359762286696208e-01,  7.392122467978273e-01,  7.414607677550722e-01,  7.428480570989009e-01,  7.435216571211178e-01,  7.436225251216276e-01,  7.432733099840527e-01,  7.425762029730666e-01,  7.416143171871158e-01},
                              {2.264297017396562e+01,  3.703766301758638e+00,  2.034998948698223e+00,  1.510923485095245e+00,  1.265729978744353e+00,  1.126910935459891e+00,  1.039315711942880e+00,  9.801156996469297e-01,  9.380990288559633e-01,  9.070002633955093e-01,  8.829463516299942e-01,  8.633779161543368e-01,  8.465599716104961e-01,  8.313215935120923e-01,  8.168794983145117e-01,  8.027015701907034e-01,  7.884022863227798e-01,  7.736657968963813e-01,  7.581862145381915e-01,  7.416143171871158e-01},
                              {1.955956459466261e+02,  1.118917023817671e+01,  4.357570503031440e+00,  2.718083521990130e+00,  2.041945502327640e+00,  1.682687096145072e+00,  1.462088170281394e+00,  1.313508264506275e+00,  1.206803763884095e+00,  1.126395471042167e+00,  1.063360967480519e+00,  1.012144436660489e+00,  9.690437805764626e-01,  9.314651792280744e-01,  8.975270882378618e-01,  8.658237613571567e-01,  8.352619776464638e-01,  8.049334692839692e-01,  7.740056420537431e-01,  7.416143171871158e-01},
                              {1.131527106972301e+03,  2.742019413138009e+01,  8.094356141096943e+00,  4.405625422851678e+00,  3.045873292912599e+00,  2.368493556832589e+00,  1.968378518204384e+00,  1.704951233806636e+00,  1.518043793772535e+00,  1.377948007790416e+00,  1.268363069256580e+00,  1.179563109954373e+00,  1.105319244270462e+00,  1.041384485194864e+00,  9.846979577532636e-01,  9.329399521299938e-01,  8.842632875709708e-01,  8.371061471443788e-01,  7.900396709438159e-01,  7.416143171871157e-01},
                              {4.991370610374878e+03,  5.832596523112534e+01,  1.361736440227531e+01,  6.617793943005997e+00,  4.277065691957527e+00,  3.176211386678905e+00,  2.549432728119129e+00,  2.146593646702069e+00,  1.865193645178458e+00,  1.656315874739094e+00,  1.493891969504980e+00,  1.362797559741365e+00,  1.253624580847262e+00,  1.160149469096889e+00,  1.078008118654219e+00,  1.003953952010710e+00,  9.354146255148074e-01,  8.702022492276336e-01,  8.062927602676150e-01,  7.416143171871157e-01},
                              {1.808482789458792e+04,  1.120299053944505e+02,  2.131886896428897e+01,  9.395528700779570e+00,  5.735282952993835e+00,  4.099439855675913e+00,  3.198582996879541e+00,  2.632582798272859e+00,  2.243339709179312e+00,  1.957469852365064e+00,  1.736744887299007e+00,  1.559416515511960e+00,  1.412280239489399e+00,  1.286729855523644e+00,  1.176933895080190e+00,  1.078670034479511e+00,  9.886802003678273e-01,  9.042295460529033e-01,  8.227686378257326e-01,  7.416143171871157e-01}},
                             {{4.738866777987514e+02,  1.684460387562540e+01,  5.619926961081758e+00,  3.281734135829232e+00,  2.397479160864624e+00,  1.959508008521145e+00,  1.708174380583837e+00,  1.550822278332539e+00,  1.447013328833976e+00,  1.376381920471174e+00,  1.327391983207241e+00,  1.292811209009341e+00,  1.267812588403031e+00,  1.249132310044230e+00,  1.234616432819130e+00,  1.222879780072204e+00,  1.213041554808854e+00,  1.204541064608597e+00,  1.197016952370690e+00,  1.190232162899990e+00},
                              {4.841681688643794e+03,  5.491635522391771e+01,  1.256979234254407e+01,  6.069209132601843e+00,  3.940274296039883e+00,  2.963447020215305e+00,  2.423693540860402e+00,  2.089182215079736e+00,  1.865572849084425e+00,  1.708118159360888e+00,  1.593041126030172e+00,  1.506471132927683e+00,  1.439628954887186e+00,  1.386580264484466e+00,  1.343153406231364e+00,  1.306371038922589e+00,  1.274091491606534e+00,  1.244744203398707e+00,  1.217124809801410e+00,  1.190232162899990e+00},
                              {3.154616792561625e+04,  1.420805372229245e+02,  2.403953052063284e+01,  9.998426062380954e+00,  5.930362539243756e+00,  4.190132768594454e+00,  3.268280841745006e+00,  2.710662024401290e+00,  2.341995909523891e+00,  2.082469140437107e+00,  1.891158929781140e+00,  1.745070641877115e+00,  1.630251730907927e+00,  1.537630629971792e+00,  1.460938380853296e+00,  1.395630981221581e+00,  1.338301797693731e+00,  1.286320343916442e+00,  1.237570697847646e+00,  1.190232162899990e+00},
                              {1.520631636586534e+05,  3.148956061770992e+02,  4.132943146104890e+01,  1.518515134801384e+01,  8.367182529059960e+00,  5.624308785058203e+00,  4.226708866462347e+00,  3.402197103627229e+00,  2.865360079281767e+00,  2.490393899977397e+00,  2.214464603850502e+00,  2.003098342270666e+00,  1.835905829230373e+00,  1.700021765831942e+00,  1.586823477367793e+00,  1.490188322141933e+00,  1.405530485165501e+00,  1.329245194088195e+00,  1.258353899045780e+00,  1.190232162899990e+00},
                              {5.901656732159231e+05,  6.246491282963873e+02,  6.581680474603525e+01,  2.173557079848703e+01,  1.125045444319795e+01,  7.254212029229660e+00,  5.287806421003054e+00,  4.154585933912857e+00,  3.428194997160839e+00,  2.925780747207696e+00,  2.557944985263177e+00,  2.276562749626175e+00,  2.053593165082403e+00,  1.871725504345519e+00,  1.719630879614922e+00,  1.589489775546923e+00,  1.475587597649461e+00,  1.373481210080780e+00,  1.279472666002594e+00,  1.190232162899990e+00},
                              {1.944624278667431e+06,  1.139848804168331e+03,  9.894809619823921e+01,  2.974824391888133e+01,  1.458002371721213e+01,  9.070365685373144e+00,  6.442950257298201e+00,  4.960971490178073e+00,  4.025088868546689e+00,  3.384287797654701e+00,  2.918103805585008e+00,  2.562588803694463e+00,  2.281050180010934e+00,  2.051085944176459e+00,  1.858294826115218e+00,  1.692973560150181e+00,  1.548256386823049e+00,  1.418980226656540e+00,  1.300924242481222e+00,  1.190232162899990e+00}},
                             {{1.890857122067037e+06,  1.074884919696010e+03,  9.039223076384690e+01,  2.645987890965103e+01,  1.274134564492299e+01,  7.864009406553027e+00,  5.591791397752693e+00,  4.343949435866960e+00,  3.580521076832391e+00,  3.077683537175252e+00,  2.729262880847459e+00,  2.479627528870858e+00,  2.297138304998906e+00,  2.162196365947915e+00,  2.061462692277420e+00,  1.985261982958638e+00,  1.926542865732524e+00,  1.880296841910385e+00,  1.843044812063057e+00,  1.812387604873647e+00},
                              {1.434546473316804e+07,  2.987011338973518e+03,  1.804473474220022e+02,  4.487048929338575e+01,  1.960113433547389e+01,  1.132727408868559e+01,  7.671280872680232e+00,  5.732691330034323e+00,  4.573075545294608e+00,  3.818589092027862e+00,  3.297130126832188e+00,  2.920640582387343e+00,  2.640274592919582e+00,  2.426998377788287e+00,  2.262233765245289e+00,  2.133064562958712e+00,  2.029912595114798e+00,  1.945516531961286e+00,  1.874392545595589e+00,  1.812387604873647e+00},
                              {7.716266115204613e+07,  6.969521346220721e+03,  3.196657990381036e+02,  6.941784107578007e+01,  2.798990029407097e+01,  1.533578393202605e+01,  9.991349773961725e+00,  7.243609507849516e+00,  5.634462725204553e+00,  4.601857009791827e+00,  3.893417077901593e+00,  3.382471282597797e+00,  2.999860062957988e+00,  2.705234908082859e+00,  2.473610569743775e+00,  2.288441176274372e+00,  2.137883347336651e+00,  2.012884307837858e+00,  1.906295529437326e+00,  1.812387604873647e+00},
                              {3.253192550565641e+08,  1.437315176424486e+04,  5.205876769957880e+02,  1.006582035946658e+02,  3.790739646062081e+01,  1.985701175129152e+01,  1.252691966593449e+01,  8.859346059355138e+00,  6.752431092162364e+00,  5.418366793527828e+00,  4.510891038980249e+00,  3.859051363710381e+00,  3.370702720510665e+00,  2.992693808833481e+00,  2.692636527934335e+00,  2.449737610939970e+00,  2.249772716121334e+00,  2.082221357100924e+00,  1.938735806854783e+00,  1.812387604873647e+00},
                              {1.143638705833100e+09,  2.703823367877713e+04,  7.964291266167922e+02,  1.391051003571698e+02,  4.935349274736288e+01,  2.486500490402286e+01,  1.525895955988075e+01,  1.056731639206889e+01,  7.918478700695184e+00,  6.262067266019560e+00,  5.144949915764652e+00,  4.346635348399592e+00,  3.749599176843221e+00,  3.286641675099088e+00,  2.917178603817272e+00,  2.615585030563546e+00,  2.364937633815368e+00,  2.153342270485199e+00,  1.971693892149562e+00,  1.812387604873647e+00},
                              {3.492208269966229e+09,  4.737075925045248e+04,  1.161019167208514e+03,  1.852377745522907e+02,  6.232811767701676e+01,  3.033836510475647e+01,  1.817240938152932e+01,  1.235792736188858e+01,  9.126360342186048e+00,  7.128676006881803e+00,  5.792462636377325e+00,  4.842756648977701e+00,  4.134472567050430e+00,  3.585273662390985e+00,  3.145733197974777e+00,  2.784907129216124e+00,  2.482804054400846e+00,  2.226062706102394e+00,  2.005149380181030e+00,  1.812387604873647e+00}}};

double stable_quick_inv_point(StableDist *dist, const double q, double *err)
{
  double x0 = 0;
  double C = 0;
  double alfa = dist->alfa;
  double beta = dist->beta;
  double q_ = q;
  double signBeta = 1;

  if (alfa<0.1)   alfa=0.1;

  if (beta < 0) {
    signBeta = -1;
    q_ = 1.0 - q_;
    beta = -beta;
  }
  if (beta==1) {
    if (q_<0.1) {
      q_=0.1;
    }
  }

  /* Asympthotic expansion near the limits of the domain */
  if (q_>0.9 || q_<0.1) {
    if (alfa!=1.0)
      C = (1-alfa)/(exp(gammaln(2-alfa))*cos(M_PI*alfa/2.0));
    else
      C = 2/M_PI;

    if (q_>0.9)
      x0=pow((1-q_)/(C*0.5*(1.0+beta)),-1.0/alfa);
    else
      x0=-pow(q_/(C*0.5*(1.0-beta)),-1.0/alfa);

    *err = 0.1;
  }

  else {
	/* Linear interpolation on precalculated values */
    int ia, ib, iq;
    double aux=0;
    double xa = modf(alfa/0.1,&aux); ia =(int)aux-1;
    double xb = modf(beta/0.2,&aux); ib =(int)aux;
    double xq = modf(  q_/0.1,&aux); iq =(int)aux-1;

    if (alfa==2) {ia = 18; xa = 1.0;}
    if (beta==1) {ib = 4;  xb = 1.0;}
    if (q_==0.9) {iq = 7;  xq = 1.0;}

    double p[8] = {precalc[iq][ib][ia],   precalc[iq][ib][ia+1],   precalc[iq][ib+1][ia],   precalc[iq][ib+1][ia+1],
                   precalc[iq+1][ib][ia], precalc[iq+1][ib][ia+1], precalc[iq+1][ib+1][ia], precalc[iq+1][ib+1][ia+1]};

	//Trilinear interpolation
    x0=((p[0]*(1.0-xa)+p[1]*xa)*(1-xb)+(p[2]*(1-xa)+p[3]*xa)*xb)*(1-xq)+((p[4]*(1.0-xa)+p[5]*xa)*(1-xb)+(p[6]*(1-xa)+p[7]*xa)*xb)*xq;

    if (err!=NULL) {
      *err = fabs(0.5*(p[0]-p[1]));
    }

#ifdef DEBUG
    printf("Quickinv: %f %f %f %d %d %d %f %f %f\n",alfa,beta,q_,ia,ib,iq,xa,xb,xq);
    printf("Precalc: \t %1.6e %1.6e \t\t %1.6e %1.6e\n\t\t %1.6e %1.6e \t\t %1.6e %1.6e",p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
#endif
  }

  x0=x0*signBeta*dist->sigma + dist->mu_0;

  return x0;
}

typedef struct {
  StableDist *dist;
  double q;
} rootparams;

double f_wrap(double x, void * params) {
  rootparams *par = (rootparams *)params;
  double y = stable_cdf_point(par->dist,x,NULL)-par->q;
#ifdef DEBUG
  printf ("F(%e)=%e\n",x,y);
#endif
  return y;
}
double df_wrap(double x, void * params) {
  rootparams *par = (rootparams *)params;
  double dy = stable_pdf_point(par->dist,x,NULL);
#ifdef DEBUG
  printf ("dF(%e)=%e\n",x,dy);
#endif
  return dy;
}
void fdf_wrap(double x, void * params, double * f, double * df) {
  rootparams *par = (rootparams *)params;
  *f  = stable_cdf_point(par->dist,x,NULL)-par->q;
  *df = stable_pdf_point(par->dist,x,NULL);
#ifdef DEBUG
  printf ("Fdf(%e)=(%e,%e)\n",x,*f,*df);
#endif
  return;
}

static int _dbl_compare (const void * a, const void * b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    return (db < da) - (da < db);
}

static short _is_guess_valid(double val)
{
    return fabs(val) < 1e-6;
}

static double _stable_inv_bracket(StableDist* dist, double q, size_t point_count, double guess, double search_width, double* guess_error)
{
    double points[point_count];
    double cdf[point_count];
    double interval_begin, interval_end, interval_step;
    size_t i, bracket_begin, bracket_end;
    double bracket_begin_val, bracket_end_val;

    interval_begin = guess - search_width / 2;
    interval_end = guess + search_width / 2;
    interval_step = search_width / point_count;

    for(i = 0; i < point_count; i++)
        points[i] = interval_begin + interval_step * i;

    stable_cdf_gpu(dist, points, point_count, cdf, NULL);

    // Binary search for the 0
    bracket_begin = 0;
    bracket_end = point_count - 1;

    bracket_begin_val = cdf[bracket_begin] - q;
    bracket_end_val = cdf[bracket_end] - q;

    if(_is_guess_valid(bracket_begin_val))
    {
        guess_error = 0;
        return points[bracket_begin];
    }
    else if(_is_guess_valid(bracket_end_val))
    {
        guess_error = 0;
        return points[bracket_end];
    }

    while(bracket_end - bracket_begin > 1)
    {
        bracket_begin_val = cdf[bracket_begin] - q;
        bracket_end_val = cdf[bracket_end] - q;

        if(bracket_begin_val > 0)
        {
            *guess_error = search_width;
            return points[bracket_begin];
        }
        else if(bracket_end_val < 0)
        {
            *guess_error = search_width;
            return points[bracket_end];
        }

        size_t middle = (bracket_end + bracket_begin + 1) / 2;
        double middle_val = cdf[middle] - q;

        if(_is_guess_valid(middle_val))
        {
            *guess_error = 0;
            return points[middle];
        }

        if(middle_val < 0)
            bracket_begin = middle;
        else
            bracket_end = middle;
    }

    *guess_error = interval_step / 2;

    return (points[bracket_end] + points[bracket_begin]) / 2;
}

double stable_inv_point_gpu(StableDist* dist, const double q, double *err)
{
    double guess;
    double interval_width;
    double tolerance = 1e-3;
    double guess_error;
    size_t point_count = 50;

    if(dist->ZONE == GAUSS || dist->ZONE == CAUCHY || dist->ZONE == LEVY)
        return stable_inv_point(dist, q, err);

    guess = stable_quick_inv_point(dist, q, &guess_error);

    if(q > 0.9 || q < 0.1)
        return guess;

    while(guess_error > tolerance)
    {
        interval_width = 2 * guess_error;
        guess = _stable_inv_bracket(dist, q, point_count, guess, interval_width, &guess_error);
    }

    if(err)
        *err = guess_error;

    return guess;
}

double stable_inv_point(StableDist *dist, const double q, double *err)
{
  double x,x0=0;

  gsl_root_fdfsolver * fdfsolver;

  // Casos particulares
  if (dist->ZONE == GAUSS) {
    x = gsl_cdf_ugaussian_Pinv(q)*M_SQRT2*dist->sigma+dist->mu_0;
    return x;
  }
  else if (dist->ZONE == CAUCHY) {
    x = tan(M_PI*(q - 0.5))*dist->sigma+dist->mu_0;
    return x;
  }
  else if (dist->ZONE == LEVY) {
    x = (dist->beta*pow(gsl_cdf_ugaussian_Pinv(q/2.0),-2.0)+dist->xi)*dist->sigma+dist->mu_0;
    return x;
  }

  x = stable_quick_inv_point(dist, q, err);

  rootparams params;
  params.dist = dist;
  params.q = q;

  gsl_function_fdf fdf;
  fdf.f   = f_wrap;
  fdf.df  = df_wrap;
  fdf.fdf = fdf_wrap;
  fdf.params = (void*)&params;

//  gsl_function f;
//  f.function = f_wrap;
//  f.params = (void*)&params;

  int status;
  if (INV_MAXITER>0) {
    fdfsolver = gsl_root_fdfsolver_alloc (gsl_root_fdfsolver_secant);
    gsl_root_fdfsolver_set (fdfsolver, & fdf, x);

//    fsolver = gsl_root_fsolver_alloc(gsl_root_fsolver_falsepos);
//    gsl_root_fsolver_set (fsolver, & f, x_lower , x_upper);

    int k=0;
	double INVrelTOL = 1e-6;
    do {
      k++;
      status = gsl_root_fdfsolver_iterate (fdfsolver);
      x0 = x;
      x = gsl_root_fdfsolver_root (fdfsolver);
      status = gsl_root_test_delta (x, x0, 0, INVrelTOL);
    } while (status == GSL_CONTINUE && k < INV_MAXITER);

    gsl_root_fdfsolver_free(fdfsolver);
  }

#ifdef DEBUG
  if (status == GSL_SUCCESS) {
    printf("convergence at x = %e\n",x);
  }
  else {
    printf("didn't converge\n");
  }
#endif

  return x;
}


void * thread_init_inv(void *ptr_args)
{
  StableArgsCdf *args = (StableArgsCdf *)ptr_args;
  int counter_ = 0;

  while (counter_ < args->Nx)
    {
      args->cdf[counter_]=(*(args->ptr_funcion))(args->dist,args->x[counter_],
                                                 &(args->err[counter_]));
      counter_++;
    }
  pthread_exit(NULL);
}

void stable_inv(StableDist *dist, const double q[], const int Nq,
                double *inv, double *err)
{
  int Nq_thread[THREADS],
      initpoint[THREADS],
      k,flag=0;
  void *status;
  pthread_t threads[THREADS];
  StableArgsCdf args[THREADS];

  /* If no error pointer is introduced, it's created*/
  if (err==NULL) {flag=1;err=malloc(Nq*sizeof(double));}

  /* Evaluation points divided among available threads */
  /* Reparto de los puntos de evaluacion entre los hilos disponibles */
  Nq_thread[0] = Nq/THREADS;
  if (0 < Nq%THREADS) Nq_thread[0]++;

  initpoint[0] = 0;
  for(k=1;k<THREADS;k++)
    {
      Nq_thread[k] = Nq/THREADS;
      if (k < Nq%THREADS) Nq_thread[k]++;
      initpoint[k] = initpoint[k-1] + Nq_thread[k-1];
    }

  /* Threads' creation. A copy of the ditribution is created for each of them*/
  /* Creacion de los hilos, pasando a cada uno una copia de la distribucion */
  for(k=0; k<THREADS; k++)
    {
      args[k].ptr_funcion = stable_inv_point;

      args[k].dist = stable_copy(dist);
      args[k].cdf  = inv+initpoint[k];
      args[k].x    = q+initpoint[k];
      args[k].Nx   = Nq_thread[k];
      args[k].err  = err+initpoint[k];

      if(pthread_create(&threads[k], NULL, thread_init_inv, (void *)&args[k]))
        {
          perror("Error en la creacion de hilo");
          if (flag==1) free(err);
          return;
        }
    }

  /* Wait until treadhs execution terminates */
  /* Esperar a finalizacion de todos los hilos */
  for(k=0; k<THREADS; k++)
    {
      pthread_join(threads[k], &status);
    }

  /* Free distribution copies */
  /* Liberar las copias de la distribucion realizadas */
  for(k=0; k<THREADS; k++)
    {
      stable_free(args[k].dist);
    }

  if (flag==1) free(err);
}
