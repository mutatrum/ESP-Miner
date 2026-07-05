import { Component, Input, ChangeDetectionStrategy } from '@angular/core';
import { NgClass } from '@angular/common';

@Component({
    selector: 'tooltip-icon',
    templateUrl: './tooltip-icon.component.html',
    styleUrls: ['./tooltip-icon.component.scss'],
    changeDetection: ChangeDetectionStrategy.Eager,
    imports: [NgClass],
    standalone: true
})
export class TooltipIconComponent {
  @Input() tooltip: string = '';
  @Input() size: string = 'xs';
  @Input() icon: string = '';

  showMobileTooltip = false;
  isMobile = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0);

  get tooltipIconClass(): string {
    return `pi ${this.icon} text-${this.size} pl-1 pr-2 tooltip-icon`;
  }
}
