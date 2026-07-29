import { Component, Input } from '@angular/core';
import { NgClass, NgFor } from '@angular/common';

@Component({
    selector: 'wifi-icon',
    templateUrl: './wifi-icon.component.html',
    styleUrls: ['./wifi-icon.component.scss'],
    imports: [NgClass, NgFor]
})
export class WifiIconComponent {
  @Input() rssi: number = 0;

  constructor() {}
}
