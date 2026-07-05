import { Component, Input, ChangeDetectionStrategy } from '@angular/core';
import { NgClass } from '@angular/common';

@Component({
    selector: 'wifi-icon',
    templateUrl: './wifi-icon.component.html',
    styleUrls: ['./wifi-icon.component.scss'],
    changeDetection: ChangeDetectionStrategy.Eager,
    imports: [NgClass]
})
export class WifiIconComponent {
  @Input() rssi: number = 0;

  constructor() {}
}
