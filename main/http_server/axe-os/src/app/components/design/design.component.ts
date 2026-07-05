import { Component, ChangeDetectionStrategy } from '@angular/core';
import { ThemeConfigComponent } from './theme-config.component';

@Component({
    selector: 'app-design',
    templateUrl: './design.component.html',
    changeDetection: ChangeDetectionStrategy.Eager,
    imports: [ThemeConfigComponent],
    standalone: true
})
export class DesignComponent {
  constructor() { }
}
