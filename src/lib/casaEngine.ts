import { AlgSqaMedDataIn, AlgSqaMedDataOut, SpermCell, MotilityGrade, MorphologyType } from '../types';

export const DEFAULT_CASA_INPUT: AlgSqaMedDataIn = {
  dRatioImg: 0.65,          // 0.65 µm / pixel (standard 10x/20x optical magnification)
  dSampleDepth: 10.0,       // 10 µm standard chamber depth
  dVolume: 3.2,             // 3.2 mL ejaculate volume
  dFrameRate: 30.0,         // 30 fps analysis
  dShapeRatio: 1.15,        // >0.9 Human mode (0.5 for QC particle, >=1.0 for porcine)
  dPlateType: 1,            // 1 = 6-chamber blue slide
  dDSDensk: 1.0,            // Density factor k
  morpPara: {
    dArea: { min: 14.0, max: 28.0 },
    dShape: { min: 1.5, max: 2.0 },
    dLength: { min: 4.0, max: 5.5 },
    dWidth: { min: 2.5, max: 3.5 },
    dCircularity: { min: 0.70, max: 0.90 }
  },
  freshSpermAreaMin: 8.0,
  freshSpermAreaMax: 50.0,
  freshSpermShapeMin: 1.05,
  freshSpermShapeMax: 2.10
};

/**
 * Generates an initial pool of sperm cells and particles for microscopic simulation
 */
export function generateInitialSpermCells(
  count: number = 65,
  width: number = 800,
  height: number = 600,
  ratioImg: number = 0.65,
  mode: 'Human' | 'Porcine' | 'Quality Control Particle' = 'Human'
): SpermCell[] {
  const cells: SpermCell[] = [];

  for (let i = 0; i < count; i++) {
    const isParticle = mode === 'Quality Control Particle' || (i % 20 === 0);
    const isRoundCell = !isParticle && (i % 15 === 0);

    let motilityClass: MotilityGrade;
    let baseSpeed: number; // in µm/s

    if (isParticle || isRoundCell) {
      motilityClass = 'D';
      baseSpeed = 0;
    } else {
      const rand = Math.random();
      if (rand < 0.38) {
        motilityClass = 'A'; // Rapid progressive >= 25 µm/s
        baseSpeed = 32 + Math.random() * 45;
      } else if (rand < 0.68) {
        motilityClass = 'B'; // Slow progressive 5 - 25 µm/s
        baseSpeed = 12 + Math.random() * 12;
      } else if (rand < 0.82) {
        motilityClass = 'C'; // Non-progressive twitching / circular
        baseSpeed = 2 + Math.random() * 3.5;
      } else {
        motilityClass = 'D'; // Immotile
        baseSpeed = 0;
      }
    }

    // Morphology determination
    let morphology: MorphologyType = 'normal';
    let headDefect: string | undefined;
    let midDefect: string | undefined;
    let tailDefect: string | undefined;

    if (isRoundCell) {
      morphology = 'round_cell';
    } else if (isParticle) {
      morphology = 'round_cell';
    } else {
      const morphRand = Math.random();
      if (morphRand < 0.18) {
        morphology = 'normal';
      } else if (morphRand < 0.45) {
        morphology = 'head_defect';
        const types = ['Amorphous head', 'Tapered head', 'Pyriform head', 'Microcephalic', 'Macrocephalic', 'Vacuolated'];
        headDefect = types[Math.floor(Math.random() * types.length)];
      } else if (morphRand < 0.65) {
        morphology = 'midpiece_defect';
        const types = ['Bent neck (>90°)', 'Thick midpiece', 'Asymmetrical insertion'];
        midDefect = types[Math.floor(Math.random() * types.length)];
      } else if (morphRand < 0.85) {
        morphology = 'tail_defect';
        const types = ['Coiled tail', 'Short tail', 'Hairpin loop', 'Broken flagellum', 'Double tail'];
        tailDefect = types[Math.floor(Math.random() * types.length)];
      } else {
        morphology = 'cytoplasmic_droplet';
        midDefect = 'Excess residual cytoplasm / CR droplet (>1/3 head size)';
      }
    }

    // Convert speed in µm/s to px/frame (at 30 fps)
    const speedPx = (baseSpeed / ratioImg) / 30.0;
    const angle = Math.random() * Math.PI * 2;
    const x = 40 + Math.random() * (width - 80);
    const y = 40 + Math.random() * (height - 80);

    const length = isRoundCell ? 6 : (4.2 + Math.random() * 1.5);
    const cellWidth = isRoundCell ? 6 : (2.8 + Math.random() * 0.8);
    const area = length * cellWidth * (Math.PI / 4) * (1 / (ratioImg * ratioImg));

    cells.push({
      id: i + 1,
      x,
      y,
      vx: Math.cos(angle) * speedPx,
      vy: Math.sin(angle) * speedPx,
      angle,
      angularVelocity: motilityClass === 'C' ? (0.2 + Math.random() * 0.4) : (Math.random() - 0.5) * 0.15,
      trail: [{ x, y, t: 0 }],
      vcl: baseSpeed * (1.1 + Math.random() * 0.3),
      vsl: motilityClass === 'A' ? baseSpeed * 0.85 : (motilityClass === 'B' ? baseSpeed * 0.65 : baseSpeed * 0.2),
      vap: motilityClass === 'A' ? baseSpeed * 0.92 : (motilityClass === 'B' ? baseSpeed * 0.78 : baseSpeed * 0.35),
      lin: motilityClass === 'A' ? 0.75 + Math.random() * 0.2 : (motilityClass === 'B' ? 0.45 + Math.random() * 0.25 : 0.15),
      str: motilityClass === 'A' ? 0.85 + Math.random() * 0.12 : (motilityClass === 'B' ? 0.65 + Math.random() * 0.2 : 0.3),
      wob: 0.78 + (Math.random() - 0.5) * 0.15,
      alh: motilityClass === 'A' ? 3.2 + Math.random() * 2.5 : (motilityClass === 'B' ? 2.1 + Math.random() * 1.8 : 0.8),
      bcf: motilityClass === 'A' ? 18 + Math.random() * 14 : (motilityClass === 'B' ? 12 + Math.random() * 8 : 4),
      mad: motilityClass === 'C' ? 65 + Math.random() * 40 : 15 + Math.random() * 20,
      motilityClass,
      morphology,
      area,
      length,
      width: cellWidth,
      circularity: isRoundCell ? 0.95 : (cellWidth / length),
      headDefectType: headDefect,
      midDefectType: midDefect,
      tailDefectType: tailDefect
    });
  }

  return cells;
}

/**
 * Computes the full CASA algorithmic parameters and diagnostics matching CountSpermMed.cpp
 */
export function computeCASAMetrics(
  cells: SpermCell[],
  input: AlgSqaMedDataIn,
  canvasWidth: number = 800,
  canvasHeight: number = 600
): AlgSqaMedDataOut {
  // Check nStatus diagnostics based on docs/nStatus-9-relax-howto.md
  let nStatus = 1;
  let statusMessage = "Normal sperm analysis completed successfully (nStatus=1).";

  if (input.dShapeRatio < 0.8) {
    nStatus = -9;
    statusMessage = "Target non-sperm or standard calibration particle mode triggered (nStatus=-9, dShapeRatio < 0.8).";
  }

  const spermOnly = cells.filter(c => c.morphology !== 'round_cell');
  const roundCells = cells.filter(c => c.morphology === 'round_cell');

  const totalSperm = spermOnly.length;
  const activeSperm = spermOnly.filter(c => c.motilityClass === 'A' || c.motilityClass === 'B' || c.motilityClass === 'C');
  const prSperm = spermOnly.filter(c => c.motilityClass === 'A' || c.motilityClass === 'B');
  const classA = spermOnly.filter(c => c.motilityClass === 'A');
  const classB = spermOnly.filter(c => c.motilityClass === 'B');
  const classC = spermOnly.filter(c => c.motilityClass === 'C');
  const classD = spermOnly.filter(c => c.motilityClass === 'D');

  // Field area in mm²: canvasWidth * canvasHeight * (ratioImg / 1000)²
  const fieldAreaMm2 = (canvasWidth * input.dRatioImg / 1000) * (canvasHeight * input.dRatioImg / 1000);
  // Field volume in mL: Area (mm²) * (depth µm / 1000 mm) * 10^-3 mL/mm³
  const fieldVolumeMl = fieldAreaMm2 * (input.dSampleDepth / 1000) * 0.001;

  // Concentration in Million / mL = (count / fieldVolumeMl) / 10^6 * dDSDensk
  const densityMultiplier = (1.0 / (fieldVolumeMl * 1_000_000)) * input.dDSDensk;
  const totalDensity = totalSperm * densityMultiplier;
  const activeDensity = (classA.length + classB.length + classC.length) * densityMultiplier;
  const prDensity = prSperm.length * densityMultiplier;
  const densityA = classA.length * densityMultiplier;
  const densityB = classB.length * densityMultiplier;
  const densityC = classC.length * densityMultiplier;
  const densityD = classD.length * densityMultiplier;
  const densityRoundcells = roundCells.length * densityMultiplier;

  // Motility percentages
  const safeTotal = totalSperm > 0 ? totalSperm : 1;
  const ratioPR = (prSperm.length / safeTotal) * 100;
  const ratioNP = (classC.length / safeTotal) * 100;
  const ratioIM = (classD.length / safeTotal) * 100;
  const activeRatio = ((prSperm.length + classC.length) / safeTotal) * 100;
  const ratioA = (classA.length / safeTotal) * 100;
  const ratioB = (classB.length / safeTotal) * 100;
  const ratioC = (classC.length / safeTotal) * 100;
  const ratioD = (classD.length / safeTotal) * 100;

  // Velocity averages
  const motileSperm = spermOnly.filter(c => c.motilityClass !== 'D');
  const motileCount = motileSperm.length > 0 ? motileSperm.length : 1;

  const aveVCL = motileSperm.reduce((acc, c) => acc + c.vcl, 0) / motileCount;
  const aveVSL = motileSperm.reduce((acc, c) => acc + c.vsl, 0) / motileCount;
  const aveVAP = motileSperm.reduce((acc, c) => acc + c.vap, 0) / motileCount;
  const aveLIN = aveVCL > 0 ? (aveVSL / aveVCL) : 0;
  const aveSTR = aveVAP > 0 ? (aveVSL / aveVAP) : 0;
  const aveWOB = aveVCL > 0 ? (aveVAP / aveVCL) : 0;
  const aveALH = motileSperm.reduce((acc, c) => acc + c.alh, 0) / motileCount;
  const aveBCF = motileSperm.reduce((acc, c) => acc + c.bcf, 0) / motileCount;
  const aveMAD = motileSperm.reduce((acc, c) => acc + c.mad, 0) / motileCount;

  // Distance parameters (for 1 second analysis window)
  const dDCL = aveVCL * 1.0;
  const dDSL = aveVSL * 1.0;
  const dDAP = aveVAP * 1.0;

  // Straight line count
  const slSperm = spermOnly.filter(c => c.str >= 0.75 && c.vsl >= 10);
  const nNumSL = slSperm.length;
  const dRatioSL = (nNumSL / safeTotal) * 100;
  const dSpermDensitySL = nNumSL * densityMultiplier;

  // Curvilinear fast count
  const clSperm = spermOnly.filter(c => c.vcl >= 20);
  const nNumCL = clSperm.length;
  const dRatioCL = (nNumCL / safeTotal) * 100;
  const dSpermDensityCL = nNumCL * densityMultiplier;

  // Velocity Histograms (10 bins: 0-10, 10-20, ..., 90-100+ µm/s)
  const histVCL = new Array(10).fill(0);
  const histVSL = new Array(10).fill(0);
  const histVAP = new Array(10).fill(0);

  spermOnly.forEach(c => {
    const binVCL = Math.min(9, Math.max(0, Math.floor(c.vcl / 10)));
    const binVSL = Math.min(9, Math.max(0, Math.floor(c.vsl / 10)));
    const binVAP = Math.min(9, Math.max(0, Math.floor(c.vap / 10)));
    histVCL[binVCL]++;
    histVSL[binVSL]++;
    histVAP[binVAP]++;
  });

  const histRank = [ratioA, ratioB, ratioC, ratioD];

  // Morphology metrics
  const normalSperm = spermOnly.filter(c => c.morphology === 'normal');
  const headDefects = spermOnly.filter(c => c.morphology === 'head_defect');
  const midDefects = spermOnly.filter(c => c.morphology === 'midpiece_defect');
  const tailDefects = spermOnly.filter(c => c.morphology === 'tail_defect');
  const crDefects = spermOnly.filter(c => c.morphology === 'cytoplasmic_droplet');

  const normalRatio = (normalSperm.length / safeTotal) * 100;
  const abnormalRatio = 100 - normalRatio;
  const hDefectsRatio = (headDefects.length / safeTotal) * 100;
  const mDefectsRatio = (midDefects.length / safeTotal) * 100;
  const tDefectsRatio = (tailDefects.length / safeTotal) * 100;
  const crDefectsRatio = (crDefects.length / safeTotal) * 100;

  const abnormalCount = (totalSperm - normalSperm.length);
  const totalDefectsCount = headDefects.length + midDefects.length + tailDefects.length + crDefects.length;
  const tzi = abnormalCount > 0 ? Number((totalDefectsCount / abnormalCount).toFixed(2)) : 1.0;
  const sdi = totalSperm > 0 ? Number((totalDefectsCount / totalSperm).toFixed(2)) : 0;

  const roundcellsRatio = (roundCells.length / (totalSperm + roundCells.length || 1)) * 100;

  return {
    nStatus,
    statusMessage,
    nTotalSpermNum: totalSperm,
    dTotaSpermDensity: Number(totalDensity.toFixed(2)),
    nActiveSpermNum: activeSperm.length,
    dActiveSpermDensity: Number(activeDensity.toFixed(2)),
    dActiveSpermRatio: Number(activeRatio.toFixed(1)),
    dRatioClassPR: Number(ratioPR.toFixed(1)),
    dRatioClassNP: Number(ratioNP.toFixed(1)),
    dRatioClassIM: Number(ratioIM.toFixed(1)),
    dRatioClassA: Number(ratioA.toFixed(1)),
    dRatioClassB: Number(ratioB.toFixed(1)),
    dRatioClassC: Number(ratioC.toFixed(1)),
    dRatioClassD: Number(ratioD.toFixed(1)),
    dDensityClassPR: Number(prDensity.toFixed(2)),
    dDensityClassA: Number(densityA.toFixed(2)),
    dDensityClassB: Number(densityB.toFixed(2)),
    dDensityClassC: Number(densityC.toFixed(2)),
    dDensityClassD: Number(densityD.toFixed(2)),
    dDensityRoundcells: Number(densityRoundcells.toFixed(2)),
    dAveVSL: Number(aveVSL.toFixed(1)),
    nNumSL,
    dRatioSL: Number(dRatioSL.toFixed(1)),
    dSpermDensitySL: Number(dSpermDensitySL.toFixed(2)),
    dAveVCL: Number(aveVCL.toFixed(1)),
    nNumCL,
    dRatioCL: Number(dRatioCL.toFixed(1)),
    dSpermDensityCL: Number(dSpermDensityCL.toFixed(2)),
    dAveVAP: Number(aveVAP.toFixed(1)),
    dLIN: Number(aveLIN.toFixed(2)),
    dSTR: Number(aveSTR.toFixed(2)),
    dWOB: Number(aveWOB.toFixed(2)),
    dALH: Number(aveALH.toFixed(2)),
    dBCF: Number(aveBCF.toFixed(1)),
    dMAD: Number(aveMAD.toFixed(1)),
    dDCL: Number(dDCL.toFixed(1)),
    dDSL: Number(dDSL.toFixed(1)),
    dDAP: Number(dDAP.toFixed(1)),
    dHistVCL: histVCL,
    dHistVSL: histVSL,
    dHistVAP: histVAP,
    dHistRank: histRank,
    dMorp: Number(normalRatio.toFixed(1)),
    dNormal: Number(normalRatio.toFixed(1)),
    dAbnormal: Number(abnormalRatio.toFixed(1)),
    dHdefects: Number(hDefectsRatio.toFixed(1)),
    dMdefects: Number(mDefectsRatio.toFixed(1)),
    dTdefects: Number(tDefectsRatio.toFixed(1)),
    dCRdefects: Number(crDefectsRatio.toFixed(1)),
    dTZI: tzi,
    dSDI: sdi,
    dRoundcells: Number(roundcellsRatio.toFixed(1)),
    nRoundcellsCount: roundCells.length
  };
}
