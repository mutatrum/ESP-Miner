import { ComponentFixture, TestBed } from '@angular/core/testing';
import { NetworkEditComponent } from './network.edit.component';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ToastrService } from 'src/app/services/toast.service';
import { DialogService } from 'src/app/services/dialog.service';

describe('NetworkEditComponent', () => {
  let component: NetworkEditComponent;
  let fixture: ComponentFixture<NetworkEditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [NetworkEditComponent],
      providers: [
        provideHttpClient(withXhr()),
        { provide: ToastrService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy(), info: jasmine.createSpy() } },
        DialogService
      ]
    });
    fixture = TestBed.createComponent(NetworkEditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
