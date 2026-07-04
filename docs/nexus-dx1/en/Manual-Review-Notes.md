# Nexus DX1 Manual Review Notes

This file summarizes editorial and technical review findings from the available first draft manual material in the repository. The requested PDF file `Manual English Version-0.pdf` was not found in the GitHub repository at the time of review; the review below is based on the available `Manual English v0 - dd1.doc` content and extracted text.

## Key Issues Found

### 1. Product name inconsistency

Several draft sections still contain references to **SCA SCOPE** or SCA-specific accessories.

Examples to correct before release:

- "Switch on the SCA SCOPE"
- "QC slides SCA SCOPE"
- "SCA Configuration Manager"
- "SCA Port", "SCA Name", or other SCA-specific terms unless they are part of a true supported interface

Recommendation: replace competitor or template product names with **Nexus DX1** or remove the section if it does not apply.

### 2. Intended use and diagnosis wording

The draft contains both in vitro diagnostic wording and non-diagnostic auxiliary wording. These statements need regulatory review so the manual uses one consistent and approved claim.

Recommendation: keep the wording conservative until regulatory claims are finalized:

> Nexus DX1 supports laboratory semen analysis workflows by acquiring and processing microscopic images of in vitro human semen samples. Final interpretation and clinical decision-making must be performed by qualified professionals.

### 3. Power specification inconsistency

The draft includes multiple power-related statements:

- AC 110 V / 240 V, 50/60 Hz
- 60 VA
- Power adapter input 100-240 V 50/60 Hz 1.4 A
- Output 12 V 5 A 60 W
- Warning text says not to insert into AC over 220 V

Recommendation: use the final product label and official power adapter rating. Avoid saying "over 220 V" if the official adapter supports up to 240 V AC.

### 4. Temperature specification needs confirmation

The draft mentions:

- Temperature control from room temperature to 60 °C
- 37 °C default
- Settings range 35 °C to 40 °C

Recommendation: confirm the actual user-adjustable temperature range and the engineering maximum. In the user manual, emphasize the user-facing routine range and default temperature.

### 5. English wording and style

Some sentences are literal translations and should be edited for customer-facing English.

Examples:

- "It’s greatly appreciated you become the user of Nexus Dx1."
- "Please, reserve all the packing materials for the use of deposit..."
- "confirm the following stuffs should be included"
- "equipment technique parameter"

Recommendation: use clear manual style:

- "Thank you for choosing Nexus DX1."
- "Keep the original packing materials for future transport or service return."
- "Check that the following items are included."
- "Technical Specifications"

### 6. Missing standard manual sections

The first draft would benefit from adding:

- Revision history
- Important notice
- Signal words: Warning, Caution, Note
- Biological safety
- Electrical safety
- Operating environment
- Routine operation overview
- Cleaning and maintenance schedule
- Transport and storage procedure
- Troubleshooting table
- Limitations and operator responsibilities
- Service and support

These sections were added in the optimized draft manual.

### 7. Figures and captions

The extracted draft contains many `[pic]` placeholders. Figures should be inserted with clear captions and cross-references.

Recommendation:

- Use figure captions such as "Figure 3. Rear interface panel".
- Avoid leaving `[pic]` placeholders in the customer-facing version.
- Confirm all screenshots match the current released software UI.

### 8. Accessories list needs final verification

The draft accessory list should be checked against the final packing list. Items such as calibration slides, QC materials, connection cables, and USB flash disks may vary by configuration.

Recommendation: add wording such as:

> The actual supplied accessories shall follow the packing list included with the instrument.

### 9. LIS and PC connection claims need confirmation

The draft states that results, videos, images, and sessions are sent to LIS. This may be too broad if not all customers have LIS configured.

Recommendation: use conditional wording:

> If LIS or PC workstation integration is configured, results may be transferred according to the installed software configuration.

### 10. Service and password recovery limitations

Password-related limitations and service contact guidance should be documented clearly. For example, if password recovery is unavailable from the user interface, the manual should instruct users to contact the distributor or service provider.

## Files Added or Updated

- `docs/nexus-dx1/en/01-Instrument-Operation-User-Manual.md`
  - Expanded customer-facing instrument operation manual.

- `docs/nexus-dx1/en/export/01-Instrument-Operation-User-Manual.docx`
  - Word review copy generated from the optimized Markdown manual.

## Remaining Information Needed

Before final PDF release, confirm:

1. Final regulatory intended-use statement.
2. Final product label power ratings.
3. Final temperature control range.
4. Final packing list.
5. Final accessory names.
6. Whether LIS/HL7/DataShare is supported in this product version.
7. Final warranty and service contact wording.
8. Official symbols used on the instrument label and package.
9. High-resolution product images and screenshots.

