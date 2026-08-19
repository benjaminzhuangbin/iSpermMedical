// Type definitions for iSperm Medical CASA System & Nexus ADB Watchdog

export interface Boundary {
  min: number;
  max: number;
}

export interface MorphologyParams {
  dArea: Boundary;         // Area (px²)
  dShape: Boundary;        // Shape (Length / Width)
  dLength: Boundary;       // Length (µm)
  dWidth: Boundary;        // Width (µm)
  dCircularity: Boundary;  // Circularity
}

export interface AlgSqaMedDataIn {
  dRatioImg: number;          // Optical ratio (µm/pixel)
  dSampleDepth: number;       // Sample chamber depth (µm)
  dVolume: number;            // Semen ejaculate volume (mL)
  dFrameRate: number;         // Camera frame rate (fps)
  dShapeRatio: number;        // Mode switch: 0.5 = Standard particle calibration, 0.9-1.2 = Human, >=1.0 = Porcine
  dPlateType: number;         // 1 = 6-chamber blue slide, 0 = 4-chamber white slide
  pcImgPath?: string;         // Image / video path
  pcResultPath?: string;      // Result path
  dDSDensk: number;           // Density correction coefficient (0.1 - 10.0)
  morpPara: MorphologyParams; // Morphology thresholds
  
  // Relaxed nStatus-9 algorithm parameters
  freshSpermAreaMin: number;
  freshSpermAreaMax: number;
  freshSpermShapeMin: number;
  freshSpermShapeMax: number;
}

export interface AlgSqaMedDataOut {
  nStatus: number;              // Status code: 1 = Success, -9 = Non-sperm/particle, -12 = Dry sample
  statusMessage: string;
  nTotalSpermNum: number;       // Total sperm count in field
  dTotaSpermDensity: number;    // Concentration (Million / mL)
  nActiveSpermNum: number;      // Motile sperm count (PR + NP)
  dActiveSpermDensity: number;  // Motile concentration (Million / mL)
  dActiveSpermRatio: number;    // Total Motility % (PR + NP)
  
  // WHO Motility Classifications
  dRatioClassPR: number;        // Progressive motility % (PR = A + B)
  dRatioClassNP: number;        // Non-progressive motility % (NP = C)
  dRatioClassIM: number;        // Immotile % (IM = D)
  dRatioClassA: number;         // Rapid Progressive (Class A, >=25 µm/s) %
  dRatioClassB: number;         // Slow Progressive (Class B, 5-25 µm/s) %
  dRatioClassC: number;         // Non-Progressive (Class C) %
  dRatioClassD: number;         // Immotile (Class D) %

  // Densities by Class (M/mL)
  dDensityClassPR: number;
  dDensityClassA: number;
  dDensityClassB: number;
  dDensityClassC: number;
  dDensityClassD: number;
  dDensityRoundcells: number;

  // Kinematic Parameters
  dAveVSL: number;              // Straight-line velocity (µm/s)
  nNumSL: number;
  dRatioSL: number;
  dSpermDensitySL: number;
  dAveVCL: number;              // Curvilinear velocity (µm/s)
  nNumCL: number;
  dRatioCL: number;
  dSpermDensityCL: number;
  dAveVAP: number;              // Average path velocity (µm/s)
  dLIN: number;                 // Linearity (VSL / VCL)
  dSTR: number;                 // Straightness (VSL / VAP)
  dWOB: number;                 // Wobble (VAP / VCL)
  dALH: number;                 // Amplitude of lateral head displacement (µm)
  dBCF: number;                 // Beat-cross frequency (Hz)
  dMAD: number;                 // Mean angular displacement (deg)
  dDCL: number;                 // Distance curvilinear (µm)
  dDSL: number;                 // Distance straight line (µm)
  dDAP: number;                 // Distance average path (µm)

  // Velocity Histograms (10 bins each: 0-10, 10-20, ..., 90-100+ µm/s)
  dHistVCL: number[];
  dHistVSL: number[];
  dHistVAP: number[];
  dHistRank: number[];          // Grade rank distribution [Class A, B, C, D]

  // Morphology Assessment (WHO Criteria)
  dMorp: number;                // Morphology normal %
  dNormal: number;              // Normal ratio %
  dAbnormal: number;            // Abnormal ratio %
  dHdefects: number;            // Head defect %
  dMdefects: number;            // Midpiece defect %
  dTdefects: number;            // Tail defect %
  dCRdefects: number;           // Cytoplasmic droplet (CR) defect %
  dTZI: number;                 // Teratozoospermia Index
  dSDI: number;                 // Sperm Deformity Index
  dRoundcells: number;          // Round cells ratio %
  nRoundcellsCount: number;     // Round cells total count
}

export type MotilityGrade = 'A' | 'B' | 'C' | 'D';
export type MorphologyType = 'normal' | 'head_defect' | 'midpiece_defect' | 'tail_defect' | 'cytoplasmic_droplet' | 'round_cell';

export interface SpermCellTrackPoint {
  x: number;
  y: number;
  t: number;
}

export interface SpermCell {
  id: number;
  x: number;
  y: number;
  vx: number;
  vy: number;
  angle: number;
  angularVelocity: number;
  trail: SpermCellTrackPoint[];
  vcl: number;
  vsl: number;
  vap: number;
  lin: number;
  str: number;
  wob: number;
  alh: number;
  bcf: number;
  mad: number;
  motilityClass: MotilityGrade;
  morphology: MorphologyType;
  area: number;
  length: number;
  width: number;
  circularity: number;
  headDefectType?: string;
  midDefectType?: string;
  tailDefectType?: string;
}

// Nexus ADB Watchdog Types
export type WatchdogDaemonStatus = 'RUNNING' | 'STOPPED' | 'RECOVERING' | 'COOLDOWN' | 'INITIALIZING';
export type SocketState = 'LISTEN' | 'ESTABLISHED' | 'CLOSE_WAIT' | 'TIME_WAIT' | 'NONE';

export interface WatchdogStatus {
  version: string;
  daemonStatus: WatchdogDaemonStatus;
  adbdRunning: boolean;
  port5555Listening: boolean;
  socketCount: number;
  establishedClients: number;
  closeWaitSockets: number;
  timeWaitSockets: number;
  rootOk: boolean;
  rootMethod: string;
  rootUid: number;
  restartCount: number;
  lastRestartReason: string;
  lastRestartTime: string;
  cooldownRemainingSeconds: number;
  uptimeSeconds: number;
  ipAddress: string;
  boardModel: string;
  androidVersion: string;
}

export interface WatchdogLogEntry {
  timestamp: string;
  level: 'INFO' | 'WARN' | 'ERROR' | 'RECOVERY' | 'DIAG';
  message: string;
}

export interface PatientInfo {
  patientId: string;
  patientName: string;
  age: number;
  abstinenceDays: number;
  collectionTime: string;
  analysisTime: string;
  sampleType: 'Human' | 'Porcine' | 'Quality Control Particle';
  referringPhysician: string;
  operatorName: string;
  notes: string;
}
