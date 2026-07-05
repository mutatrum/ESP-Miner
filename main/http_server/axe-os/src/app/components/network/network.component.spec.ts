import { ComponentFixture, TestBed } from '@angular/core/testing';
import { NetworkComponent } from './network.component';
import { NetworkEditComponent } from '../network-edit/network.edit.component';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ToastrService } from 'src/app/services/toast.service';
import { DialogService } from 'src/app/services/dialog.service';

describe('NetworkComponent', () => {
  let component: NetworkComponent;
  let fixture: ComponentFixture<NetworkComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
    imports: [NetworkComponent, NetworkEditComponent],
      providers: [
        provideHttpClient(withXhr()),
        { provide: ToastrService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy(), info: jasmine.createSpy() } },
        DialogService
      ]
    });
    fixture = TestBed.createComponent(NetworkComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
